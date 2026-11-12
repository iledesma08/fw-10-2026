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

## Hito 8 — Cinco comprobaciones

Las direcciones de datos siguientes corresponden a APP_SRC="print.c test_data_bss.c"; no se mezclan con la imagen final de test_fw_status.

1. **HEX y mapas:** BL en línea 1; magic del header en línea 1025. Dirección=4×(N−1). Campo CRC en 0x100C/línea 1028; puntero de reset en 0x1014/línea 1030. El log usa índice de palabra hexadecimal: 0x400 es dirección 0x1000, no dirección 0x400.
2. **Inspección:** objdump -d del BL muestra j a bl_reset_handler en cero y j a bl_unexpected en 0x10. objdump -s -j .fw_header del app muestra los seis campos. El ELF todavía contiene FFFFFFFF en image_crc; el HEX contiene el CRC calculado.
3. **Entrada:** símbolo Reset_Handler=0x1018, Entry point=0x1018 y palabra en 0x1014=0x1018. El reset físico del core sigue siendo cero; el BL lee ese campo y salta.
4. **Datos:** .data tiene VMA 0x20000, LMA 0x16CC y tamaño 0x14. _sdata=0x20000, _edata=0x20014 y _sidata=0x16CC. .bss ocupa [0x20014,0x20058), tamaño 0x44. La imagen llega hasta 0x16E0 y __fw_size=0x6E0. El test compara valores y declara PASS.
5. **Carga:** después de simular esa misma imagen, make -C fw check informó RESULTADO: OK. Se compararon palabras del HEX con loading del log. Eso verifica la carga y los predicados del script, no sustituye el test de ejecución.

Comandos de inspección:

```sh
make -C fw inspect APP_SRC="print.c test_data_bss.c"
riscv64-unknown-elf-readelf -S -l -W fw/build/app.elf
riscv64-unknown-elf-nm -n fw/build/app.elf
make -C fw check
```

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
