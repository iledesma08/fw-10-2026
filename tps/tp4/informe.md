# TP4

### Excepción del RTL

En `rtl/testbench.sv`, la señal `wire trap;` se encontraba desconectada. Los escenarios de fallo como la ejecución en modo standalone de `test_data_bss` imprimían `FAIL` y ejecutaban `ebreak`, pero la simulación no concluía y continuaba indefinidamente. La situación se resolvió conectando la señal del procesador mediante `wire trap = u_top.trap;`.

## Hito 1 — Reconocimiento del arranque original

### 1. Primera instrucción, ubicación y orden de enlace

```sh
make -C fw clean
make -C fw STANDALONE=1 APP_SRC="print.c test_fw_status.c"
head -n 1 fw/build/fw.hex # Primera palabra del hex
riscv64-unknown-elf-objdump -d fw/build/standalone.elf
riscv64-unknown-elf-nm -n fw/build/standalone.elf
```

- Se observa como primera palabra a `00040117` en dirección 0x0, que se desensambla como `auipc sp,0x40`. La instrucción siguiente conserva el valor mediante `mv sp,sp` y juntas implementan `la sp,_stack_top`, que es la primera instruccion de la seccion `_start` de `startup.S`.

- `_start` queda en cero porque FLASH empieza en cero, el script reúne .text y la receta pasa startup.S antes de print.c. ENTRY(_start) escribe en la cabecera ELF que la entrada del programa es el valor de ese símbolo. No ubica automáticamente el símbolo, no ordena moverlo al principio de FLASH ni configura el reset del hardware. El Pico de este TP empieza físicamente en 0x0 por su configuración en `rtl/top.v`.

- Al enlazar una variante temporal con print.c antes de startup.S, uart0_send.part.0 queda en 0x0 y `_start` en 0x1d8 (fue desplazado). De esta forma, ENTRY seguira apuntando a `_start`, ahora en 0x1d8, pero el core seguiría empezando en 0x0: ejecutaría una rutina distinta, sin haber preparado el entorno. Por eso importa ubicar explícitamente el arranque.

### 2. Secciones huérfanas y globales

Una sección huérfana es una sección de entrada que el linker script (`linker.ld`) no indica dónde colocar. El linker puede crearle una sección de salida y elegir su ubicación. No significa que se haya perdido ni que la compilación necesariamente falle.

```sh
make -C fw clean
make -C fw STANDALONE=1 APP_SRC="print.c test_fw_status.c" LDFLAGS="-Wl,--gc-sections -Wl,--orphan-handling=warn"
riscv64-unknown-elf-objdump -t fw/build/standalone.elf
```

El enlace con linker.ld informó como secciones huerfanas a:

```console
.sbss
.riscv.attributes
.comment
.debug_line
.debug_line_str
.debug_info
.debug_abbrev
.debug_aranges
.debug_str
.debug_loclists
.debug_rnglists
.debug_frame.
```  

Para ver donde quedaron las variables globales:

```sh
riscv64-unknown-elf-objdump -t fw/build/standalone.elf
```

`-t` muestra la tabla de símbolos, incluida la sección a la que pertenece cada uno:

```console
00020008 g     O .sbss  00000004 int_0
00020000 g     O .sbss  00000004 int_2
00020004 g     O .sbss  00000004 int_1
```

De esta forma, las variables se encuentran en `.sbss` (en las direcciones 0x20008, 0x20004 y 0x20000 dentro de RAM). La sección ocupa doce bytes y es NOBITS, lo que significa que ocupa espacio en memoria sin almacenar esos bytes como contenido de esa sección en el ELF. El linker la colocó en RAM, pero eso no ejecuta ninguna instrucción para ponerla en cero.

El patrón .bss* en `linker.ld` no captura .sbss porque esta última comienza por .s, por eso termina siendo huerfana.

### 3. Comparación con "De 0 a main"

El arranque original `startup.S` únicamente inicializa el puntero de pila (`la sp, _stack_top`), salta de inmediato a `main` mediante `call main` y queda en un bucle. Para dar a un programa C el estado inicial que espera, faltan dos tareas:

- Copiar los datos inicializados (.data y .sdata) en ROM/FLASH a RAM.
- Limpiar .bss/.sbss: en C, las variables globales sin inicializador deben comenzar en cero. Reservar sus direcciones en el ELF no limpia la RAM.

A pesar de esta deficiencia, la simulación standalone de `test_fw_status` termina reportando éxito con 0xFF. Este comportamiento constituye un **falso positivo** que se explica por tres factores

1. **El test no valida resultados:** `test_fw_status.c` realiza operaciones aritméticas iterativas sobre sus variables globales (`int_0`, `int_1`, `int_2`), pero no incluye aserciones ni verifica si los resultados numéricos obtenidos son correctos. Al concluir el bucle, escribe incondicionalmente `REG_FW_STATUS = 0xFF`.

2. **No utiliza variables con valores iniciales (`.data`):** el test solo declara variables no inicializadas (ubicadas en `.sbss`). Al no existir datos iniciales distintos de cero que requieran ser preservados o copiados desde la imagen ROM, la ausencia del bucle de copia de `.data` pasa totalmente inadvertida.

3. **El simulador enmascara la falta de inicialización:** el testbench precarga el archivo `fw.hex` exclusivamente en la memoria de instrucciones (`memory_i`), dejando intacta la memoria de datos (`memory_d` / RAM). En Verilator, la RAM no inicializada arranca por defecto con todos sus bits en cero (`0x00000000`); por eso en el simulador las variables de `.sbss` ya leían cero, ocultando la falta de limpieza del startup. En un chip físico real, una SRAM al energizarse contiene valores residuales aleatorios

En conclusión, la observación de 0xFF sólo demuestra que `test_fw_status` completó su flujo programado y **no certifica que el entorno de C esté correctamente inicializado**. Esta deficiencia queda evidenciada en el **Hito 6**, donde el test `test_data_bss.c`, que sí exige variables inicializadas no nulas y comprueba sus valores antes de declarar éxito, falla de manera determinista bajo el startup original y solo pasa al incorporar la rutina completa desarrollada en este trabajo.

## Hito 6 — Inicialización de .data y .bss

Para verificar si el entorno de C se inicializa correctamente, se utiliza el test `test_data_bss.c`. A diferencia del test inicial, este define variables globales con valores iniciales (`tp4_data_word = 0x1234ABCD` en `.sdata` y `tp4_data_array` en `.data`) y variables en cero (`.sbss`/`.bss`), declaradas como `volatile`, que le indica al compilador que conserve cada lectura y escritura efectiva en memoria, evitando que el optimizador suprima los accesos.

El programa comprueba cada valor y solo escribe `0xFF` (`PASS`) si todos coinciden, ante cualquier discrepancia imprime `FAIL`, escribe `0xBAD` y ejecuta `ebreak` (`TRAP!`).

Para comparar el comportamiento, se simula el mismo test bajo dos caminos de arranque:

1. **Combinado:** Bootloader + `app_startup.S`, que implementa los bucles de copia de `.data`/`.sdata` a RAM y puesta a cero de `.bss`/`.sbss`.
2. **Standalone (`STANDALONE=1`):** arranque original `startup.S`, que solo carga `sp` y salta a `main` sin inicializar la memoria.

```sh
make -C fw sim APP_SRC="print.c test_data_bss.c" VER_FLAGS="--binary -Wno-fatal -f filelist.f"
make -C fw sim STANDALONE=1 APP_SRC="print.c test_data_bss.c" VER_FLAGS="--binary -Wno-fatal -f filelist.f"
```

| Variante | Valor observado `tp4_data_word` | Resultado observado | Ciclos |
|---|---|---|---|
| **Combinada** | `0x1234ABCD` | `PASS`, estado `0xFF` | 776217 |
| **Standalone** | `0x00000000` | `FAIL` (en `.sdata` y `.data`), `TRAP!` | 16028 |

#### ¿Por qué falla el arranque original?

El flujo standalone falla porque `startup.S` **no copia las secciones de datos desde ROM a RAM**. Como consecuencia, las variables globales leen el contenido residual de la RAM (`0x00000000` en el simulador) en lugar de sus valores iniciales, provocando que el test detecte el error, escriba `0xBAD` y detenga la simulación mediante `ebreak`.

> *Nota sobre `.bss`:* Que las variables sin inicializar lean cero en standalone no demuestra que hayan sido limpiadas, sino que coincide con el estado inicial en ceros de la memoria RAM en Verilator. La ausencia de copia en `.data`/`.sdata` es lo que demuestra de forma inequívoca la deficiencia del arranque original.

## Hito 8

Al principio me costó entender qué era exactamente lo que se me pedía en este hito. De hecho, fue el punto donde más me trabé: no terminaba de ver para qué servía calcular números de línea ni qué estaba haciendo en realidad. Por eso puede que sea muy largo porque, principalmente, me sirve a mi hacerlo asi.

Al profundizar en la consigna y en el código entendí que en sistemas embebidos existe una distancia enorme entre **lo que el linker cree que generó** y **lo que físicamente termina cargado en la memoria de la CPU**. Que una compilación o un script empaquetador terminen sin errores no garantiza que los bytes hayan quedado donde el procesador los espera.

El Hito 8 existe para contrastar fuentes independientes sin confiar ciegamente en ninguna:

1. **El archivo ELF y los mapas (`bl.map`, `app.map`):** Indican dónde planificó el linker ubicar cada sección, función y variable.

2. **El archivo empaquetado (`fw.hex`):** Es la imagen plana final armada por `mkimage.py`. Cada línea contiene 8 caracteres hexadecimales, lo que equivale a $8 \times 4\text{ bits} = 32\text{ bits} = 4\text{ bytes}$ (exactamente una palabra de RISC-V). El testbench va a leer cada linea desde el principio con init_memory()

3. **El log del simulador (`sim.log`):** Registra palabra por palabra cómo la función `init_memory()` del testbench fue escribiendo la memoria simulada desde la dirección cero.

4. **La ejecución real:** Lo que la CPU hace cuando empieza a ejecutar instrucciones desde el reset.

Osea, el corazón de este hito es auditar de punta a punta el contrato entre el Bootloader y el Firmware. Por ejemplo: verificar que si el linker ubicó `Reset_Handler` en `0x1018`, ese valor figure efectivamente en el campo `reset_handler` del header en `0x1014`, que el testbench cargue esa palabra en `0x1014`, y que el Bootloader lea esa posición para saltar al código de la aplicación.

Después de entender globalmente el hito, ya pude hacer cada paso mejor:

### Paso 1: Ubicación en memoria y relación entre Líneas, Índices y Direcciones

El primer paso consistió en localizar el Bootloader y el header del Firmware dentro de la imagen integrada `fw.hex`.

El testbench carga la memoria secuencialmente palabra por palabra desde la dirección `0x0`. Por lo tanto:

- **Línea 1 del HEX:** Corresponde a la palabra en el índice `0`, es decir, la dirección `0x0000`, donde arranca el salto inicial del Bootloader (coincidiendo con `memory_map.ld` para `BL_ROM`).

- **Línea 1025 del HEX:** Corresponde al inicio del Firmware (`APP_ROM`). Antes de esta línea hay $1025 - 1 = 1024$ palabras. Al multiplicar 1024 palabras por 4 bytes cada una, obtenemos 4096 bytes, que en hexadecimal es exactamente `0x1000` (base definida en `memory_map.ld`).

#### La relación entre el log (`sim.log`) y el archivo (`fw.hex`)

Al inspeccionar `sim.log`, el testbench imprime trazas con el siguiente formato:

```text
loading: 00000400 -> 46345054
```

Acá el valor `00000400` no es una dirección en bytes, sino el **índice de palabra en hexadecimal**:

- $0x400_{\text{hex}} = 1024_{\text{dec}}$ palabras cargadas previamente.
- Para pasarlo a dirección física en bytes: $0x400 \times 4\text{ bytes} = \mathbf{0x1000}$.
- Coincide exactamente con la línea $1024 + 1 = \mathbf{1025}$ del archivo `fw.hex`.

#### De Dirección a Línea: El puntero de reset

Para hacer el camino inverso tomamos, por ejemplo, el campo del puntero de reset en la dirección $A = 0x1014$ (definido en `fw_image.h` como offset `0x14` dentro del header):

- Convertimos a bytes en decimal: $0x1014 = 4116\text{ bytes}$.
- Índice de palabra: $i = \frac{A}{4} = \frac{4116}{4} = 1029 = \mathbf{0x405_{\text{hex}}}$.
- Número de línea en el archivo de texto: $N = i + 1 = 1029 + 1 = \mathbf{1030}$.

Al mirar la línea 1030 del archivo `fw.hex` y el log en el índice `0x405`, vemos exactamente el mismo valor:

```text
loading: 00000405 -> 00001018
```

Aparece el mismo número (`00001018`) porque **la línea 1030 del `.hex` es el dato que el testbench lee e imprime en el log**. Y ese valor `0x1018` es el contenido del campo: la dirección de memoria donde empieza `Reset_Handler`.

> Esto significa que en `0x1014` hay un **puntero** cuyo valor es `0x1018`. En `0x1018` residen las instrucciones de código. El Bootloader lee el contenido de `0x1014` y salta a `0x1018`. **Si confundiéramos dirección con contenido, el procesador intentaría ejecutar el propio header como si fuera código.**

En `app/app_startup.S` se declara primero la seccion `.fw_header` que mide 24 bytes, osea va de `0x1000` a `0x1017`, y lo proximo que se define es `Reset_Handler`, por eso queda en `0x1018`.

El salto se produce porque `.word Reset_Handler` le indica al linker que en esa parte del header ponga la direccion del simbolo `Reset_Handler`, y como cada palabra ocupa 4 bytes y esta es la sexta palabra desde el inicio de la ROM para la app, esta en `0x1014`. Por eso el linker escribe 0x1018 en 0x1014, luego el Bootloader lee esta direccion y le ordena a la CPU saltar ahi.

#### ¿Que hay de `0x1000` a `0x1003`?

En la línea 1025 de `fw.hex` esta la palabra `46345054`:

- En la tabla ASCII: `'T'` = `0x54`, `'P'` = `0x50`, `'4'` = `0x34`, `'F'` = `0x46`.
- En una arquitectura Little-Endian como RISC-V, el byte menos significativo se almacena en la dirección de memoria más baja:
  - Dirección `0x1000`: `0x54` (`'T'`)
  - Dirección `0x1001`: `0x50` (`'P'`)
  - Dirección `0x1002`: `0x34` (`'4'`)
  - Dirección `0x1003`: `0x46` (`'F'`)
- Cuando la CPU lee esos 4 bytes juntos como una palabra entera de 32 bits, el valor numérico es `0x46345054`.

### Paso 2: Mirar instrucciones y datos con herramientas distintas

- Para ver código ejecutable se usa **desensamblado** (`objdump -d`).
- Para ver tablas, headers y constantes se usa `objdump -s`.

```bash
$ riscv64-unknown-elf-objdump -d fw/build/bl.elf
$ riscv64-unknown-elf-objdump -s .fw_header fw/build/app.elf
```

Al desensamblar `bl.elf` con `-d`, vemos las instrucciones reales de la tabla de vectores definida en `bl_start.S`:

```console
00000000 <_bl_vectors>:
   0:   0140006f                j       14 <bl_reset_handler>
   4:   0100006f                j       14 <bl_unexpected>
   ...
```

Se observa el salto inicial en `0x0000` hacia `0x14`. Esto ocurre porque la tabla de vectores reserva 5 palabras de 4 bytes (`0x00`, `0x04`, `0x08`, `0x0C` y `0x10`) para interrupciones, juystamente como `bl_reset_handler`, por lo que la lógica real del Bootloader comienza inmediatamente después, en el offset `0x14`.

Sobre `app.elf` usamos `-s .fw_header` para ver los bytes en crudo, que podemos ver cosas como el TP4F que dije antes.

### Paso 3: Comprobar el puntero de reset desde fuentes distintas

Para comprobar que el firmware no vaya a saltar a cualquier lado al arrancar, contrastamos fuentes independientes:

```bash
$ riscv64-unknown-elf-readelf -h fw/build/app.elf   # Entry point del ELF -> 0x1018
$ riscv64-unknown-elf-nm -n fw/build/app.elf        # Dirección del símbolo Reset_Handler -> 0x1018
$ sed -n '1030p' fw/build/fw.hex                    # Linea 1030 (offset 0x1014) en fw.hex -> 0x1018
```

Los tres valores coinciden en **`0x1018`**. Esto valida que el linker, el compilador y el empaquetador están alineados.

> *Nota:* se puede saber en la linea 1030 esta el campo `reset_handler` porque se usa la misma formula que antes. En `fw_image.h` se define que en el offset 0x14 esta `reset_handler`, que en memoria seria la direccion $0x1014 = 4116$ bytes, por lo tanto la cantidad de palabras previas es $4116 / 4 = 1029$, que es la linea 1030 en `fw.hex`

### Paso 4: Comprobar la inicialización de memoria (VMA vs LMA)

Acá se analiza cómo viajan las variables globales inicializadas (`.data`) y las que van a cero (`.bss`):

```bash
$ riscv64-unknown-elf-readelf -S -l fw/build/app.elf
$ riscv64-unknown-elf-nm -n fw/build/app.elf
$ make -C fw sim APP_SRC="print.c test_data_bss.c" CORRUPT=0
```

Con `readelf -l` se observó la diferencia entre **VMA (Virtual Memory Address)** y **LMA (Load Memory Address)** para la sección de `.data`:

- En ejecución, `.data` vive en la RAM (`VirtAddr` en el rango `0x00020000`).
- Pero en la imagen almacenada, sus valores iniciales residen en la ROM (`PhysAddr` dentro del bloque de `APP_ROM` que empieza en  `0x1000`).
- Los símbolos `_sidata`, `_sdata` y `_edata` delimitaban exactamente ese rango de copia, mientras que `_sbss` y `_ebss` marcaban el bloque de RAM que el startup debía limpiar a cero.

`test_data_bss.c` verifica que la variable con valor `0x1234ABCD` y el arreglo `{1, 2, 3, 4}` estan correctamente copiados en RAM, y que las variables no inicializadas son efectivamente cero.

### Paso 5: Comparar la carga efectiva con `make check`

El último paso cierra la auditoría automatizando la comparación de todo lo anterior:

```bash
$ make -C fw check
```

Se toma el archivo empaquetado `fw/build/fw.hex` y lo compara palabra por palabra contra las líneas `loading: ...` registradas en `fw/build/sim.log`. No simula de nuevo, sino que audita el log que ya existe. Por eso debe ejecutarse inmediatamente después de una simulación limpia y válida.

Este tipo de tests nos permiten confirmar que **el transporte y la carga de la imagen en la memoria del core fueron exitosos**. Si el test hubiese fallado con un `FAIL` pero `make check` diera `OK`, sabríamos con certeza que el problema no es de empaquetado ni de carga en memoria, sino un error de lógica en el código C o en los bucles de copia de `Reset_Handler`.

> Osea, mi conclusion es que siempre debe existir un archivo o algo que haga toda esta auditoria y contraste entre distintas fuentes de forma automatizada para verificar que todo salio bien. Y esto nos permite diagnosticar si un problema fue en el empaquetado, la carga o algo de la app.

## Hito 9 — Preguntas del informe

### 1. PicoRV32 frente a Cortex-M

PicoRV32 comienza a ejecutar instrucciones desde PROGADDR_RESET; por eso necesita un salto en 0x0. En Cortex-M del ejemplo de clase, el hardware lee la primera palabra como SP inicial y la segunda como dirección de reset. Son direcciones, no instrucciones de salto. El header del FW almacena datos de ese estilo, pero aquí los interpreta el BL por software. No se atribuye mtvec al mecanismo específico de este PicoRV32.

### 2. Pasos del Makefile

GCC compila y enlaza bl.elf y app.elf según scripts diferentes; objcopy extrae binarios; mkimage comprueba el formato, completa CRC, agrega relleno y combina; hexdump representa palabras; install copia a fw/fw.hex; sim ejecuta Verilator y el testbench carga ese archivo.

**¿Por qué ELF separados?** BL y FW tienen regiones, entradas y responsabilidades diferentes. Se pueden inspeccionar sin resolver accidentalmente sus funciones entre sí. El contrato de transferencia es el header más la ABI del salto.

**¿Qué dirección usa objcopy -O binary para .data?** La LMA; por eso los bytes iniciales quedan dentro de la imagen ROM y no se crea un enorme hueco hasta su VMA de RAM. No conserva tablas de símbolos ni depuración como datos del programa.

**¿Qué agrega mkimage entre BL y FW?** Bytes cero hasta load_addr=0x1000. Si se ejecutan, esas palabras son instrucciones ilegales en esta configuración. También puede agregar relleno final hasta múltiplo de cuatro y completar el CRC dentro del header.

### 3. Diferencia en la palabra 0x100C

app.elf contiene el marcador FW_IMAGE_CRC_UNSET=0xFFFFFFFF. El empaquetador escribe el CRC calculado en el binario combinado después del enlace, y no modifica el ELF. En la imagen final sana se observó 0x000000F7; en la variante test_data_bss, 0x00000097. La discrepancia con el marcador del ELF es intencional. El campo es de 32 bits aunque el CRC tenga ocho; sus bits altos deben quedar en cero.

### 4. Comparar el FW anterior y el relinkeado

Se compararon instrucciones de los programas generados desde las mismas fuentes locales, sin traer soluciones de otra rama. El standalone empieza con su propio startup en cero; la imagen nueva añade el header y el startup completo en 0x1018. No se pueden comparar las líneas del FW como si solo se hubiera agregado un desplazamiento fijo: hay código añadido y secciones reorganizadas.

Comparación concreta medida: print_str empieza en 0xB4 en standalone y en 0x1130 en el FW nuevo. Su instrucción lbu a5,0(a0) mantiene la palabra 00054783; su salto final mantiene F35FF06F aunque cambia el destino absoluto de 0x10 a 0x108C. En main, la dirección de la cadena inicial cambia: standalone usa li a0,928 (0x3A0); el FW usa lui a0,0x1 y addi a0,a0,1064 para formar 0x1428. El salto de dirección absoluta de la cadena necesita instrucciones distintas y cambia el tamaño/orden local.

En una función de C equivalente, operaciones sobre registros como lbu a5,0(a0) conservan codificación si no cambian sus operandos. Saltos relativos pueden conservar su desplazamiento si origen y destino se mueven juntos; referencias absolutas deben ajustarse a nuevas direcciones. La relajación del linker también puede cambiar instrucciones. El ELF y el HEX no son intercambiables: el HEX nuevo ya incluye el CRC post-enlace.

### 5. jr frente a jalr ra y recarga de stack

bl_jump es un traspaso sin retorno. jr a1 equivale a jalr x0,0(a1), no guarda una dirección de vuelta al BL. Cambiar sp abandona la pila del BL: no se debe intentar volver al epílogo C anterior. ra se limpia para no conservar un retorno residual, pero el startup sigue siendo responsable de manejar la vuelta de main.

El FW recarga sp para establecer por sí mismo el entorno de entrada, incluso si BL dejó el mismo valor. Esto no inicializa .data ni .bss: esas tareas tienen bucles separados.

### 6. Éxito y error

El BL no escribe 0xFF porque el testbench lo interpreta como finalización exitosa y podría terminar antes de ejecutar el FW. La aplicación declara su éxito. 0xBAD solo comunica fallo: la condición del testbench es 0xFF o trap. Por eso se necesita ebreak/bl_halt, y fue necesario conectar correctamente trap en el testbench mediante la excepción autorizada.

### 7. Funciones de strutils y ausencia de libc

La versión local de strutils.c solo define str_crc8. nm del BL confirma que queda str_crc8: bl_main la llama. -ffunction-sections separa funciones y --gc-sections conserva las alcanzables y elimina las no usadas; compilar un archivo no obliga a incluir todo su contenido. Si se usara una biblioteca más extensa, el resultado dependería de sus referencias y optimización. El BL no enlaza libc; su CRC no llama a malloc, printf ni otras funciones de esa biblioteca. -lgcc aporta auxiliares del compilador, no libc.

### 8. Compresión de instrucciones

rv32ic permite instrucciones de dos bytes, y un ensamblador puede comprimir saltos. Si la tabla supone cinco instrucciones consecutivas de cuatro bytes y no usa .option norvc, el salto de IRQ podría no quedar en 0x10. La tabla del BL fuerza instrucciones de cuatro bytes y el flujo actual lo compila rv32i; no se modificó COMPRESSED_ISA del RTL para este TP.

## Cierre de entrega y límites

Logs reales exigidos: [simulación sana](entrega-sano.log) y [simulación corrupta](entrega-corrupto.log). La imagen final queda sana, con el test_fw_status por defecto; se regeneró y se repitieron sim y check después de CORRUPT=1. Resultado final: 0xFF y RESULTADO: OK, 497229 ciclos. Los artefactos de fw/build corresponden a ese último flujo sano; los números del hito 8 con test_data_bss están anotados arriba como una variante diferente.

Los tests no certifican autenticidad ni ausencia de todos los errores; el CRC protege integridad accidental. No se realizó el extra opcional de rendimiento ni se modificaron parámetros de CPU. La reserva de pila de 4096 bytes es estática y no constituye análisis de peor caso.
