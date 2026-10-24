# TP2 — Material completo

> Registro de mediciones y notas técnicas. No es la entrega final; la versión corta está en `tp2.md`.

## Cómo se midió

- Docker `fw-env`, con el flujo normal: `make clean`, `make build_project_config`, `make -C fw`, `make sim`.
- Para que las 200 iteraciones no tardaran una eternidad, en la copia de trabajo se comentaron `$dumpfile` y `$dumpvars` del testbench.
  - **Por qué eso no cambia los ciclos ni los accesos:** `$dumpfile` solo indica el archivo de salida y `$dumpvars` registra los valores de las señales; son tareas de observación, no forman parte de la lógica del circuito ni del testbench. No alteran el reloj, los contadores de ciclos ni las transacciones de memoria; solo agregan trabajo de escritura a disco. Se verificó comparando los casos 1 y 2 con los logs hechos con VCD: dieron exactamente 13.876 y 210.132 ciclos, con los mismos accesos.
- **Qué es un trap:** es una excepción síncrona que genera el CPU cuando encuentra una instrucción que no puede ejecutar (por ejemplo, una instrucción ilegal o no soportada por la configuración del RTL). El core aborta esa instrucción y activa la señal `trap`. En este testbench, cuando `trap` se activa, la simulación se corta y se informa el ciclo en que ocurrió. Por eso, en las filas 3 y 6, “ciclos” significa “ciclos hasta el trap”, no el tiempo de un programa completo.

### Problema encontrado con el testbench

Durante las filas 3 y 6 la simulación no terminaba: el VCD crecía hasta cientos de MB y el PC quedaba dando vueltas, pero nunca aparecía `TRAP!`. Al revisar el testbench se encontró que declaraba `wire trap;` pero nunca lo conectaba a `u_top.trap`. El módulo `top` tampoco expone `trap` como puerto: la señal existe dentro de `top` (viene de `picorv32`), pero el testbench miraba otra señal local que quedaba sin driver.

**Qué significa “señal local sin driver”:** en Verilog/SystemVerilog, un `wire` es un cable que tiene que estar manejado por algo (`assign`, un puerto de salida o la salida de una instancia). Si nadie lo maneja, queda flotando (`z`/indefinido). El testbench declaraba su propio `wire trap;`, y el `top` tenía otro `wire trap;` interno conectado a `picorv32`. Son dos cables distintos aunque se llamen igual: cada uno vive en su módulo. El testbench leía su cable local, que no estaba conectado a nada, así que nunca veía el trap del CPU. Como el bucle de espera usa `while (... && trap == 0)`, esa condición nunca cambiaba y la simulación no salía por trap.

La solución fue agregar `assign trap = u_top.trap;` en el testbench. Es una referencia jerárquica, igual que otras que el testbench ya usa (por ejemplo, `u_top.u_register_file.REGISTER_FW_STATUS`). Con ese cambio, las filas 3 y 6 imprimen `TRAP!` y el ciclo correspondiente: 12 y 1.506. Es un problema de infraestructura del testbench, no del CPU. Si no se permite modificarlo, el síntoma visible es un cuelgue en vez de un trap.

## Repaso de ISA, `rv32i`/`rv32ic`/`rv32imc`, picorv32 y PCPI

- **RISC-V** es una ISA modular: hay una base obligatoria y extensiones opcionales. El nombre `rv32i` significa “RISC-V de 32 bits con la base I”. `rv32ic` agrega C; `rv32imc` agrega M y C.
- **I (Integer base):** instrucciones enteras básicas: loads/stores (`lw`, `sw`), aritmética (`add`, `addi`, `sub`), lógica (`and`, `or`, `xor`), shifts, saltos (`jal`, `jalr`, `beq`, `bne`, etc.). No incluye multiplicación ni división.
- **M (Multiplication/Division):** agrega `mul`, `mulh`, `mulhsu`, `mulhu` (multiplicación y variantes con signo), `div`/`divu` (división con/sin signo) y `rem`/`remu` (resto con/sin signo). Si el compilador usa `-march=rv32imc`, puede emitir estas instrucciones; si el RTL no las implementa, el programa trapea o se cuelga.
- **C (Compressed):** agrega versiones de 16 bits de instrucciones comunes (por ejemplo `c.addi`, `c.lw`, `c.sw`, `c.j`, `c.mv`). No cambia la semántica: es la misma operación en menos bytes. Sirve para achicar código; no necesariamente para correr más rápido. El RTL necesita `COMPRESSED_ISA(1)` para decodificarlas.
- **`rv32i`:** base I, sin M ni C. Para multiplicar/dividir, GCC llama a rutinas de software de libgcc.
- **`rv32ic`:** base I + C. Sigue sin multiplicador/divisor de hardware; el código puede ser más chico, pero las operaciones matemáticas siguen siendo por software.
- **`rv32imc`:** base I + M + C. GCC puede usar `mul`/`div`/`rem` de hardware y también instrucciones comprimidas.

**Sobre `-march`:** `-march=rv32imc` no configura el RTL: solo le dice a GCC que puede generar instrucciones de las extensiones M y C. El RTL no cambia; sigue con lo que digan `ENABLE_MUL`, `ENABLE_DIV` y `COMPRESSED_ISA`. Lo que cambia es el binario: si GCC mete `mul`/`div`/comprimidas y el RTL no las tiene, el core no las puede ejecutar. La relación es indirecta: `-march` decide el contenido del binario; los parámetros del RTL deciden si ese contenido se puede ejecutar.

- **picorv32:** es el core RISC-V que está instanciado en `rtl/top.v` como `u_picorv32`. No es el testbench ni el FW: es el procesador. Sus parámetros (`ENABLE_MUL`, `ENABLE_FAST_MUL`, `ENABLE_DIV`, `COMPRESSED_ISA`, etc.) deciden qué instrucciones implementa.
- **PCPI:** “Pico Co-Processor Interface”. Es una interfaz de picorv32 para derivar instrucciones a un coprocesador. El multiplicador y el divisor internos de picorv32 se implementan como cores PCPI. `WITH_PCPI` es un `localparam` interno que vale 1 si hay algún PCPI habilitado (`ENABLE_PCPI`, `ENABLE_MUL`, `ENABLE_FAST_MUL` o `ENABLE_DIV`). Cuando `WITH_PCPI=1`, el core intenta mandar al PCPI las instrucciones que no reconoce, en vez de trapear de inmediato; si no hay un core que responda, puede quedarse esperando.

## Repaso de libgcc, ELF y `.text`

- **libgcc:** es la biblioteca de soporte de GCC. Tiene rutinas en software para operaciones que el CPU no hace en hardware:

| Rutina | Qué hace |
|---|---|
| `__mulsi3` | multiplicación signed de 32 bits |
| `__divsi3` | división signed de 32 bits |
| `__udivsi3` | división unsigned de 32 bits |
| `__modsi3` | resto signed de 32 bits |
| `__umodsi3` | resto unsigned de 32 bits |

- **`print_dec`:** recibe `unsigned int val` y hace `val % 10` y `val / 10`. Como es unsigned, GCC llama a `__umodsi3` y `__udivsi3`, no a las versiones con signo.
- **ELF:** es el archivo ejecutable que genera el linker (`fw/current.elf`). Tiene secciones: `.text` (código), `.rodata` (constantes), `.data` (datos inicializados), `.bss` (datos no inicializados), tabla de símbolos, etc. `size`, `nm` y `objdump` leen ese archivo.
- **`.text`:** es la sección de código. “Tamaño de FW = última instrucción” significa mirar la dirección más alta de instrucción en `.text`, no el tamaño total del archivo ni el tamaño de `.text` en bytes. El `size` de `.text` puede incluir datos de solo lectura/alineación según el linker; por eso el tamaño en bytes no coincide exactamente con la última dirección.

**Corrección sobre `__modsi3`:** la frase “el binario usa `__modsi3` para `/10` y `%10`” no es correcta. `print_dec` usa las versiones unsigned (`__umodsi3`/`__udivsi3`); se ve en el `objdump`: `jal 328 <__umodsi3>` y `jal 2e0 <__hidden___udivsi3>`. ¿Por qué aparece `__modsi3`? Porque libgcc trae también las versiones con signo (`__divsi3`, `__modsi3`), el linker las incluye en el ELF y las ubica después de `main`; en este binario la última función de `.text` es `__modsi3`, y por eso la última instrucción es `0x388`. No significa que `print_dec` la llame.

## Por qué las filas 7 y 8 necesitan `ENABLE_DIV`

`rv32imc` = RV32I + M + C. La M agrega `mul`, `div`, `divu`, `rem` y `remu`; la C agrega instrucciones comprimidas. Al compilar con `-march=rv32imc`, GCC puede emitir `divu`/`remu`. `print_dec(unsigned int val)` hace `val % 10` y `val / 10`, así que GCC usa `remu`/`divu` en lugar de llamar a `__umodsi3`/`__udivsi3`.

Picorv32 tiene `ENABLE_MUL` y `ENABLE_DIV` separados: `ENABLE_MUL=1` no prende el divisor. Además, con `ENABLE_MUL=1` el core queda con `WITH_PCPI=1` y manda las instrucciones que no reconoce al PCPI; como `ENABLE_DIV=0`, no hay divisor que responda y la simulación se queda en `remu` (PC `0xb0`). Al agregar `ENABLE_DIV(1)`, el divisor interno responde y el programa termina. Por eso las filas 7 y 8 llevan DIV además de MUL/FAST_MUL.

## Sobre `du obj_dir`

`du obj_dir` cuenta bytes en disco de lo que generó Verilator: C++ generado, objetos, binario, runtime y, si se usa `--trace`, el código de trazado. **No es área de síntesis**: no cuenta celdas ni recursos de FPGA; para eso habría que sintetizar. La consigna lo pide como estimación porque es fácil de obtener, pero es una métrica ruidosa: depende del compilador, flags, debug y trace. Sirve solo para comparar muy grossero. Ejemplo: la fila 8 ocupa menos bytes que la fila 7 aunque el multiplicador rápido debería usar más hardware; eso muestra que no se puede leer como área.

## Tabla

| Caso | Flag march | Config de RTL (`top.v`) | Tamaño de FW (última instrucción) | Ciclos totales | Uso de celdas (`du obj_dir`) | Accesos a memoria (datos / instrucciones) |
|---|---|---|---|---|---|---|
| Original | `rv32i` | original | `0x388` (977 B de `.text`) | 13.876 | 3,6 M (3.545.767 B) | 894 / 3.893 |
| Aumentar iteraciones x10 | `rv32i` | original | `0x388` (977 B) | 210.132 | 3,6 M (3.545.767 B) | 8.414 / 59.105 |
| Anterior | `rv32ic` | original | `0x2a4` (749 B) | 12 (TRAP) | 3,6 M (3.545.767 B) | 0 / 4 |
| Anterior | `rv32ic` | `COMPRESSED_ISA(1)` | `0x2a4` (749 B) | 214.367 | 3,7 M (3.737.784 B) | 8.414 / 52.872 |
| Agregar multiplicaciones | `rv32ic` | anterior (C=1) | `0x2d4` (797 B) | 238.167 | 3,7 M (3.737.784 B) | 8.414 / 59.472 |
| Anterior | `rv32imc` | anterior (C=1, sin MUL) | `0x1ee` (565 B) | 1.506 (TRAP) | 3,7 M (3.737.784 B) | 103 / 348 |
| Anterior | `rv32imc` | `ENABLE_MUL(1)` + `ENABLE_DIV(1)` | `0x1ee` (565 B) | 149.376 | 3,9 M (3.921.578 B) | 7.214 / 18.985 |
| Anterior | `rv32imc` | `ENABLE_FAST_MUL(1)` + `ENABLE_DIV(1)` | `0x1ee` (565 B) | 128.976 | 3,9 M (3.848.082 B) | 7.214 / 18.985 |

Notas de la tabla:

- El “tamaño de FW” es la dirección de la última instrucción, como pide la consigna. Entre paréntesis es el tamaño de `.text`.
- En las filas 1 a 5 la última instrucción cae dentro de las rutinas de división/módulo de libgcc; en las filas 6 a 8 cae en `main`, porque con `rv32imc` GCC usa multiplicación/división por hardware y no necesita esas rutinas.
- Los ciclos de las filas 3 y 6 son “hasta el trap”, no el tiempo de un programa completo.

## Observaciones por fila

### Fila 1 — Original (`rv32i`, RTL original)

- Es la línea base: el programa usa solo instrucciones base y libgcc para división/módulo. La última instrucción cae en `__modsi3` porque el linker ubica ahí las rutinas de libgcc, no porque `print_dec` llame a esa función.

### Fila 2 — Iteraciones x10 (`rv32i`)

- El cuerpo del `for` se repite 10 veces, pero el tiempo no escala 10×: los ciclos y los accesos a instrucciones crecen ~15×, mientras que los datos crecen ~9,4×.
- La diferencia se explica por `print_dec`: el valor acumulado crece muy rápido (la recurrencia lo multiplica por ~4,24 en cada iteración) y la impresión divide por 10 en software. Con un número más grande, esas divisiones y el envío por UART cuestan más, así que el resto del programa no escala igual que el `for`.

### Fila 3 — `rv32ic` con RTL original

- El FW ya usa instrucciones comprimidas, pero el RTL no las decodifica; el core trapea en la primera instrucción comprimida, antes de `main`. Por eso no hay accesos a datos y el ciclo es mínimo.
- Sin el arreglo de `trap`, el síntoma es un cuelgue; el trap existe, pero el testbench no lo ve.

### Fila 4 — `COMPRESSED_ISA(1)`

- Al habilitar compressed, el programa termina. La reducción de tamaño de código se nota en el binario y baja el fetch de instrucciones respecto de la fila 2, pero los ciclos no bajan: el ahorro es de memoria, no de tiempo.

### Fila 5 — Multiplicaciones con `rv32ic`

- Como `rv32ic` no tiene M, GCC resuelve las multiplicaciones con `__mulsi3`. El programa anda, pero el costo es alto: respecto de la fila 4, la multiplicación por software agrega ~23.800 ciclos y más instrucciones. El binario también crece por la rutina.

### Fila 6 — `rv32imc` con RTL anterior (sin multiplicador)

- Hay un desalineamiento: GCC emite `mul` porque `rv32imc` incluye M, pero el RTL no tiene multiplicador. El core trapea en la primera multiplicación; el ciclo es chico comparado con una corrida completa y los accesos son solo los previos al trap.
- El FW se achica porque ya no arrastra `__mulsi3`.

### Fila 7 — `ENABLE_MUL(1)` + `ENABLE_DIV(1)`

- Con el multiplicador en hardware, el programa termina y el costo baja fuerte frente a la multiplicación por software (fila 5): menos ciclos y menos accesos a datos, porque desaparece `__mulsi3`.
- `ENABLE_DIV` es necesario para que `print_dec` no se cuelgue en `remu`; no alcanza con MUL.

### Fila 8 — `ENABLE_FAST_MUL(1)` + `ENABLE_DIV(1)`

- Mismo FW y mismos accesos que la fila 7; solo cambia la latencia del multiplicador. `ENABLE_FAST_MUL` reduce los ciclos ~13,7% respecto de la fila 7, sin cambiar el comportamiento de memoria.
- El `du` no es una buena medida de área: puede dar menor aunque el multiplicador rápido use más hardware.

## Por qué no escala lineal (detalle)

- El `for` se repite 10 veces más, pero el valor que se acumula no crece linealmente: cada iteración combina los dos enteros anteriores, así que el valor se multiplica aproximadamente por 4,24 en cada vuelta (y además desborda 32 bits).
- `print_dec` imprime ese valor haciendo `%10` y `/10` en un bucle; con `rv32i` esas operaciones son rutinas de software (`__umodsi3`/`__udivsi3`) que tardan más cuanto más grande es el número.
- Se detectó comparando los cocientes entre fila 1 y fila 2. Si todo escalara con el `for`, ciclos, datos e instrucciones darían ~10×. Dieron ~15× en ciclos e instrucciones y ~9,4× en datos. Esa diferencia no se explica solo por el bucle; se fue a mirar `print_dec` y se vio que hace división por 10 en software.

## Cosas que conviene explicar en la entrega

- **Tamaño del FW:** `-march` decide qué instrucciones puede usar GCC. `C` reemplaza instrucciones de 32 bits por versiones comprimidas de 16 bits, así que achica el código. `M` hace que GCC use `mul`/`div` de hardware y elimina las rutinas de libgcc (`__mulsi3`, `__udivsi3`, etc.), que también achican el binario. La “última instrucción” incluye startup, `main` y las rutinas de libgcc que el linker haya agregado; por eso en `rv32i`/`rv32ic` termina en `__modsi3` y en `rv32imc` termina en `main`.
- **Por qué no siempre baja el tiempo:** menos bytes de código no significa menos ciclos. `C` reduce el fetch de instrucciones, pero el core no acelera por tener instrucciones más chicas; además, el workload está dominado por `print_dec` y el UART, que no cambian con `C`. En la fila 4 se ve que baja el fetch pero los ciclos quedan iguales o apenas peores. La ISA comprimida es una optimización de memoria, no de velocidad.
- **Multiplicación por software vs hardware:** en `rv32ic` no existe M, así que GCC llama a `__mulsi3`, una rutina de software con shifts, sumas y accesos a memoria; funciona, pero es lenta. En `rv32imc`, GCC emite `mul`; si el RTL tiene `ENABLE_MUL`, se ejecuta en hardware y el costo baja. La fila 5 muestra el costo de `__mulsi3`; la fila 7 muestra la mejora de `mul`; la fila 6 muestra qué pasa si el FW espera hardware que el RTL no tiene.
- **División:** `rv32imc` incluye M, y M incluye `div`/`divu`/`rem`/`remu`. GCC asume que existen y las emite para `/` y `%`. `print_dec` usa `/10` y `%10`, así que necesita división aunque el usuario no escriba ninguna. Si el RTL solo habilita MUL, `WITH_PCPI=1` hace que el core derive `divu/remu` al PCPI; como no hay divisor, se queda esperando en `remu`. Por eso hay que habilitar `ENABLE_DIV(1)`.
- **Trap vs cuelgue:** un trap es una excepción síncrona: el CPU aborta la instrucción y activa `trap` porque encontró algo que no puede ejecutar. En la fila 3, la instrucción comprimida no está soportada (`COMPRESSED_ISA=0`); en la fila 6, `mul` no está soportada (`ENABLE_MUL=0`). En ambos casos el core trapea. Pero el testbench original no conecta `trap`, así que el síntoma visible es un cuelgue. En las filas 7/8 sin `ENABLE_DIV`, en cambio, el core no trapea de inmediato: deriva `divu/remu` al PCPI y se queda esperando. Son dos “no anda” distintos.
- **`du obj_dir`:** es una estimación pedida por la consigna, no una medición de celdas. Cuenta el modelo generado por Verilator, objetos, binario, runtime y trace. Es ruidosa y depende de flags; sirve para comparar muy grossero, no para decidir área. En la fila 8 da menos que en la fila 7 aunque el multiplicador rápido debería ocupar más hardware, lo que muestra su limitación.

## Conclusión (borrador)

Los resultados muestran que el rendimiento y el tamaño del firmware dependen de la combinación entre lo que el compilador puede emitir (`-march`) y lo que el RTL realmente implementa. Pasar de `rv32i` a `rv32ic` reduce el tamaño del código porque usa instrucciones comprimidas, pero no reduce los ciclos: en la fila 4 baja el fetch de instrucciones y, sin embargo, el tiempo queda igual o apenas peor. La ISA comprimida sirve para ahorrar memoria, no para acelerar.

El cambio a `rv32imc` sí tiene impacto en el tiempo cuando el RTL acompaña. Con `rv32ic`, las multiplicaciones se resuelven con `__mulsi3` y el costo es alto; con `rv32imc` y `ENABLE_MUL(1)`, GCC usa `mul` de hardware y el programa baja de 238.167 a 149.376 ciclos, además de reducir los accesos a datos porque desaparece la rutina de software. `ENABLE_FAST_MUL` mejora todavía más la latencia (128.976 ciclos), sin cambiar el comportamiento de memoria; el costo es más hardware, aunque `du obj_dir` no sea una buena medida de área.

La división es el caso más claro de configuración que “no anda”: `rv32imc` incluye M, y GCC asume que `divu/remu` existen; `print_dec` usa `/10` y `%10`, así que necesita división aunque el usuario no la escriba. Si el RTL solo habilita MUL, el core deriva `divu/remu` al PCPI y se queda esperando porque no hay divisor; por eso hay que habilitar también `ENABLE_DIV(1)`. Algo parecido pasa con `rv32ic` sin `COMPRESSED_ISA(1)` y con `rv32imc` sin `ENABLE_MUL(1)`: el FW pide instrucciones que el RTL no tiene y el core trapea. El testbench original además no conecta `trap`, así que el síntoma visible es un cuelgue en vez de un trap; corregir esa conexión permite medir el ciclo exacto del trap.

En resumen, `rv32i` es la configuración más simple y compatible; `rv32ic` conviene cuando el tamaño de código es crítico; `rv32imc` con MUL/DIV conviene cuando hay multiplicaciones o divisiones en el camino caliente y se puede pagar el hardware; `ENABLE_FAST_MUL` conviene si la latencia del multiplicador importa más que el área. La condición común es mantener alineados el `-march` del firmware y los parámetros del RTL; si no, el sistema trapea o se cuelga. La métrica `du obj_dir` sirve solo como estimación gruesa y no debe interpretarse como área de síntesis.
