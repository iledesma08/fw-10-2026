# TP4

### Excepcion del RTL

En `rtl/testbench.sv`, la senal `wire trap;` se encontraba desconectada. Los escenarios de fallo como la ejecucion en modo standalone de `test_data_bss` imprimian `FAIL` y ejecutaban `ebreak`, pero la simulacion no concluia y continuaba indefinidamente. La situacion se resolvio conectando la senal del procesador mediante `wire trap = u_top.trap;`.

## Hito 1 - Reconocimiento del arranque original

### 1. Primera instruccion, ubicacion y orden de enlace

```sh
make -C fw clean
make -C fw STANDALONE=1 APP_SRC="print.c test_fw_status.c"
head -n 1 fw/build/fw.hex # Primera palabra del hex
riscv64-unknown-elf-objdump -d fw/build/standalone.elf
riscv64-unknown-elf-nm -n fw/build/standalone.elf
```

- Se observa como primera palabra a `00040117` en direccion 0x0, que se desensambla como `auipc sp,0x40`. La instruccion siguiente conserva el valor mediante `mv sp,sp` y juntas implementan `la sp,_stack_top`, que es la primera instruccion de la seccion `_start` de `startup.S`

- `_start` queda en cero porque FLASH empieza en cero, el script reune .text y la receta pasa startup.S antes de print.c. ENTRY(_start) escribe en la cabecera ELF que la entrada del programa es el valor de ese simbolo. No ubica automaticamente el simbolo, no ordena moverlo al principio de FLASH ni configura el reset del hardware. El Pico de este TP empieza fisicamente en 0x0 por su configuracion en `rtl/top.v`

- Al enlazar una variante temporal con print.c antes de startup.S, uart0_send.part.0 queda en 0x0 y `_start` en 0x1d8 (fue desplazado). De esta forma, ENTRY seguira apuntando a `_start`, ahora en 0x1d8, pero el core seguiria empezando en 0x0: ejecutaria una rutina distinta, sin haber preparado el entorno. Por eso importa ubicar explicitamente el arranque

### 2. Secciones huerfanas y globales

Una seccion huerfana es una seccion de entrada que el linker script (`linker.ld`) no indica donde colocar. El linker puede crearle una seccion de salida y elegir su ubicacion. No significa que se haya perdido ni que la compilacion necesariamente falle.

```sh
make -C fw clean
make -C fw STANDALONE=1 APP_SRC="print.c test_fw_status.c" LDFLAGS="-Wl,--gc-sections -Wl,--orphan-handling=warn"
riscv64-unknown-elf-objdump -t fw/build/standalone.elf
```

El enlace con linker.ld informo como secciones huerfanas a:

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

`-t` muestra la tabla de simbolos, incluida la seccion a la que pertenece cada uno:

```console
00020008 g     O .sbss  00000004 int_0
00020000 g     O .sbss  00000004 int_2
00020004 g     O .sbss  00000004 int_1
```

De esta forma, las variables se encuentran en `.sbss` (en las direcciones 0x20008, 0x20004 y 0x20000 dentro de RAM). La seccion ocupa doce bytes y es NOBITS, lo que significa que ocupa espacio en memoria sin almacenar esos bytes como contenido de esa seccion en el ELF. El linker la coloco en RAM, pero eso no ejecuta ninguna instruccion para ponerla en cero.

El patron .bss* en `linker.ld` no captura .sbss porque esta ultima comienza por .s, por eso termina siendo huerfana.

### 3. Comparacion con "De 0 a main"

El arranque original `startup.S` unicamente inicializa el puntero de pila (`la sp, _stack_top`), salta de inmediato a `main` mediante `call main` y queda en un bucle. Para dar a un programa C el estado inicial que espera, faltan dos tareas:

- Copiar los datos inicializados (.data y .sdata) en ROM/FLASH a RAM
- Limpiar .bss/.sbss: en C, las variables globales sin inicializador deben comenzar en cero. Reservar sus direcciones en el ELF no limpia la RAM

A pesar de esta deficiencia, la simulacion standalone de `test_fw_status` termina reportando exito con 0xFF. Este comportamiento constituye un **falso positivo** que se explica por tres factores

1. **El test no valida resultados:** `test_fw_status.c` realiza operaciones aritmeticas iterativas sobre sus variables globales (`int_0`, `int_1`, `int_2`), pero no incluye aserciones ni verifica si los resultados numericos obtenidos son correctos. Al concluir el bucle, escribe incondicionalmente `REG_FW_STATUS = 0xFF`

2. **No utiliza variables con valores iniciales (`.data`):** el test solo declara variables no inicializadas (ubicadas en `.sbss`). Al no existir datos iniciales distintos de cero que requieran ser preservados o copiados desde la imagen ROM, la ausencia del bucle de copia de `.data` pasa totalmente inadvertida

3. **El simulador enmascara la falta de inicializacion:** el testbench precarga el archivo `fw.hex` exclusivamente en la memoria de instrucciones (`memory_i`), dejando intacta la memoria de datos (`memory_d` / RAM). En Verilator, la RAM no inicializada arranca por defecto con todos sus bits en cero (`0x00000000`); por eso en el simulador las variables de `.sbss` ya leian cero, ocultando la falta de limpieza del startup. En un chip fisico real, una SRAM al energizarse contiene valores residuales aleatorios

En conclusion, la observacion de 0xFF solo demuestra que `test_fw_status` completo su flujo programado y **no certifica que el entorno de C este correctamente inicializado**. Esta deficiencia queda evidenciada en el **Hito 6**, donde el test `test_data_bss.c`, que si exige variables inicializadas no nulas y comprueba sus valores antes de declarar exito, falla de manera determinista bajo el startup original y solo pasa al incorporar la rutina completa desarrollada en este trabajo.

## Hito 6 - Inicializacion de .data y .bss

Para verificar si el entorno de C se inicializa correctamente, se utiliza el test `test_data_bss.c`. A diferencia del test inicial, este define variables globales con valores iniciales (`tp4_data_word = 0x1234ABCD` en `.sdata` y `tp4_data_array` en `.data`) y variables en cero (`.sbss`/`.bss`), declaradas como `volatile`, que le indica al compilador que conserve cada lectura y escritura efectiva en memoria, evitando que el optimizador suprima los accesos

El programa comprueba cada valor y solo escribe `0xFF` (`PASS`) si todos coinciden, ante cualquier discrepancia imprime `FAIL`, escribe `0xBAD` y ejecuta `ebreak` (`TRAP!`)

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

#### ¿Por que falla el arranque original?

El flujo standalone falla porque `startup.S` **no copia las secciones de datos desde ROM a RAM**. Como consecuencia, las variables globales leen el contenido residual de la RAM (`0x00000000` en el simulador) en lugar de sus valores iniciales, provocando que el test detecte el error, escriba `0xBAD` y detenga la simulacion mediante `ebreak`

> *Nota sobre `.bss`:* Que las variables sin inicializar lean cero en standalone no demuestra que hayan sido limpiadas, sino que coincide con el estado inicial en ceros de la memoria RAM en Verilator. La ausencia de copia en `.data`/`.sdata` es lo que demuestra de forma inequivoca la deficiencia del arranque original.

## Hito 8

Al principio me costo entender que era exactamente lo que se me pedia en este hito. De hecho, fue el punto donde mas me trabe: no terminaba de ver para que servia calcular numeros de linea ni que estaba haciendo en realidad. Por eso puede que sea muy largo porque, principalmente, me sirve a mi hacerlo asi.

Al profundizar en la consigna y en el codigo entendi que en sistemas embebidos existe una distancia enorme entre **lo que el linker cree que genero** y **lo que fisicamente termina cargado en la memoria de la CPU**. Que una compilacion o un script empaquetador terminen sin errores no garantiza que los bytes hayan quedado donde el procesador los espera

El Hito 8 existe para contrastar fuentes independientes sin confiar ciegamente en ninguna:

1. **El archivo ELF y los mapas (`bl.map`, `app.map`):** Indican donde planifico el linker ubicar cada seccion, funcion y variable.

2. **El archivo empaquetado (`fw.hex`):** Es la imagen plana final armada por `mkimage.py`. Cada linea contiene 8 caracteres hexadecimales, lo que equivale a $8 \times 4\text{ bits} = 32\text{ bits} = 4\text{ bytes}$ (exactamente una palabra de RISC-V). El testbench va a leer cada linea desde el principio con init_memory()

3. **El log del simulador (`sim.log`):** Registra palabra por palabra como la funcion `init_memory()` del testbench fue escribiendo la memoria simulada desde la direccion cero.

4. **La ejecucion real:** Lo que la CPU hace cuando empieza a ejecutar instrucciones desde el reset

Osea, el corazon de este hito es auditar de punta a punta el contrato entre el Bootloader y el Firmware. Por ejemplo: verificar que si el linker ubico `Reset_Handler` en `0x1018`, ese valor figure efectivamente en el campo `reset_handler` del header en `0x1014`, que el testbench cargue esa palabra en `0x1014`, y que el Bootloader lea esa posicion para saltar al codigo de la aplicacion

Despues de entender globalmente el hito, ya pude hacer cada paso mejor:

### Paso 1: Ubicacion en memoria y relacion entre Lineas, Indices y Direcciones

El primer paso consistio en localizar el Bootloader y el header del Firmware dentro de la imagen integrada `fw.hex`.

El testbench carga la memoria secuencialmente palabra por palabra desde la direccion `0x0`. Por lo tanto:

- **Linea 1 del HEX:** Corresponde a la palabra en el indice `0`, es decir, la direccion `0x0000`, donde arranca el salto inicial del Bootloader (coincidiendo con `memory_map.ld` para `BL_ROM`)

- **Linea 1025 del HEX:** Corresponde al inicio del Firmware (`APP_ROM`). Antes de esta linea hay $1025 - 1 = 1024$ palabras. Al multiplicar 1024 palabras por 4 bytes cada una, obtenemos 4096 bytes, que en hexadecimal es exactamente `0x1000` (base definida en `memory_map.ld`)

#### La relacion entre el log (`sim.log`) y el archivo (`fw.hex`)

Al inspeccionar `sim.log`, el testbench imprime trazas con el siguiente formato:

```text
loading: 00000400 -> 46345054
```

Aca el valor `00000400` no es una direccion en bytes, sino el **indice de palabra en hexadecimal**:

- $0x400_{\text{hex}} = 1024_{\text{dec}}$ palabras cargadas previamente
- Para pasarlo a direccion fisica en bytes: $0x400 \times 4\text{ bytes} = \mathbf{0x1000}$.
- Coincide exactamente con la linea $1024 + 1 = \mathbf{1025}$ del archivo `fw.hex`.

#### De Direccion a Linea: El puntero de reset

Para hacer el camino inverso tomamos, por ejemplo, el campo del puntero de reset en la direccion $A = 0x1014$ (definido en `fw_image.h` como offset `0x14` dentro del header):

- Convertimos a bytes en decimal: $0x1014 = 4116\text{ bytes}$.
- Indice de palabra: $i = \frac{A}{4} = \frac{4116}{4} = 1029 = \mathbf{0x405_{\text{hex}}}$.
- Numero de linea en el archivo de texto: $N = i + 1 = 1029 + 1 = \mathbf{1030}$.

Al mirar la linea 1030 del archivo `fw.hex` y el log en el indice `0x405`, vemos exactamente el mismo valor:

```text
loading: 00000405 -> 00001018
```

Aparece el mismo numero (`00001018`) porque **la linea 1030 del `.hex` es el dato que el testbench lee e imprime en el log**. Y ese valor `0x1018` es el contenido del campo: la direccion de memoria donde empieza `Reset_Handler`.

> Esto significa que en `0x1014` hay un **puntero** cuyo valor es `0x1018`. En `0x1018` residen las instrucciones de codigo. El Bootloader lee el contenido de `0x1014` y salta a `0x1018`. **Si confundieramos direccion con contenido, el procesador intentaria ejecutar el propio header como si fuera codigo.**

En `app/app_startup.S` se declara primero la seccion `.fw_header` que mide 24 bytes, osea va de `0x1000` a `0x1017`, y lo proximo que se define es `Reset_Handler`, por eso queda en `0x1018`

El salto se produce porque `.word Reset_Handler` le indica al linker que en esa parte del header ponga la direccion del simbolo `Reset_Handler`, y como cada palabra ocupa 4 bytes y esta es la sexta palabra desde el inicio de la ROM para la app, esta en `0x1014`. Por eso el linker escribe 0x1018 en 0x1014, luego el Bootloader lee esta direccion y le ordena a la CPU saltar ahi.

#### ¿Que hay de `0x1000` a `0x1003`?

En la linea 1025 de `fw.hex` esta la palabra `46345054`:

- En la tabla ASCII: `'T'` = `0x54`, `'P'` = `0x50`, `'4'` = `0x34`, `'F'` = `0x46`.
- En una arquitectura Little-Endian como RISC-V, el byte menos significativo se almacena en la direccion de memoria mas baja:
  - Direccion `0x1000`: `0x54` (`'T'`)
  - Direccion `0x1001`: `0x50` (`'P'`)
  - Direccion `0x1002`: `0x34` (`'4'`)
  - Direccion `0x1003`: `0x46` (`'F'`)
- Cuando la CPU lee esos 4 bytes juntos como una palabra entera de 32 bits, el valor numerico es `0x46345054`.

### Paso 2: Mirar instrucciones y datos con herramientas distintas

- Para ver codigo ejecutable se usa **desensamblado** (`objdump -d`).
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

Se observa el salto inicial en `0x0000` hacia `0x14`. Esto ocurre porque la tabla de vectores reserva 5 palabras de 4 bytes (`0x00`, `0x04`, `0x08`, `0x0C` y `0x10`) para interrupciones, juystamente como `bl_reset_handler`, por lo que la logica real del Bootloader comienza inmediatamente despues, en el offset `0x14`.

Sobre `app.elf` usamos `-s .fw_header` para ver los bytes en crudo, que podemos ver cosas como el TP4F que dije antes

### Paso 3: Comprobar el puntero de reset desde fuentes distintas

Para comprobar que el firmware no vaya a saltar a cualquier lado al arrancar, contrastamos fuentes independientes:

```bash
$ riscv64-unknown-elf-readelf -h fw/build/app.elf   # Entry point del ELF -> 0x1018
$ riscv64-unknown-elf-nm -n fw/build/app.elf        # Direccion del simbolo Reset_Handler -> 0x1018
$ sed -n '1030p' fw/build/fw.hex                    # Linea 1030 (offset 0x1014) en fw.hex -> 0x1018
```

Los tres valores coinciden en **`0x1018`**. Esto valida que el linker, el compilador y el empaquetador estan alineados

> *Nota:* se puede saber en la linea 1030 esta el campo `reset_handler` porque se usa la misma formula que antes. En `fw_image.h` se define que en el offset 0x14 esta `reset_handler`, que en memoria seria la direccion $0x1014 = 4116$ bytes, por lo tanto la cantidad de palabras previas es $4116 / 4 = 1029$, que es la linea 1030 en `fw.hex`

### Paso 4: Comprobar la inicializacion de memoria (VMA vs LMA)

Aca se analiza como viajan las variables globales inicializadas (`.data`) y las que van a cero (`.bss`):

```bash
$ riscv64-unknown-elf-readelf -S -l fw/build/app.elf
$ riscv64-unknown-elf-nm -n fw/build/app.elf
$ make -C fw sim APP_SRC="print.c test_data_bss.c" CORRUPT=0
```

Con `readelf -l` se observo la diferencia entre **VMA (Virtual Memory Address)** y **LMA (Load Memory Address)** para la seccion de `.data`:

- En ejecucion, `.data` vive en la RAM (`VirtAddr` en el rango `0x00020000`)
- Pero en la imagen almacenada, sus valores iniciales residen en la ROM (`PhysAddr` dentro del bloque de `APP_ROM` que empieza en  `0x1000`)
- Los simbolos `_sidata`, `_sdata` y `_edata` delimitaban exactamente ese rango de copia, mientras que `_sbss` y `_ebss` marcaban el bloque de RAM que el startup debia limpiar a cero

`test_data_bss.c` verifica que la variable con valor `0x1234ABCD` y el arreglo `{1, 2, 3, 4}` estan correctamente copiados en RAM, y que las variables no inicializadas son efectivamente cero

### Paso 5: Comparar la carga efectiva con `make check`

El ultimo paso cierra la auditoria automatizando la comparacion de todo lo anterior:

```bash
$ make -C fw check
```

Se toma el archivo empaquetado `fw/build/fw.hex` y lo compara palabra por palabra contra las lineas `loading: ...` registradas en `fw/build/sim.log`. No simula de nuevo, sino que audita el log que ya existe. Por eso debe ejecutarse inmediatamente despues de una simulacion limpia y valida

Este tipo de tests nos permiten confirmar que **el transporte y la carga de la imagen en la memoria del core fueron exitosos**. Si el test hubiese fallado con un `FAIL` pero `make check` diera `OK`, sabriamos con certeza que el problema no es de empaquetado ni de carga en memoria, sino un error de logica en el codigo C o en los bucles de copia de `Reset_Handler`.

> Osea, mi conclusion es que siempre debe existir un archivo o algo que haga toda esta auditoria y contraste entre distintas fuentes de forma automatizada para verificar que todo salio bien. Y esto nos permite diagnosticar si un problema fue en el empaquetado, la carga o algo de la app

## Hito 9 - Preguntas del informe

### 1. PicoRV32 frente a Cortex-M

- **PicoRV32:** Al salir de reset, el hardware inicializa directamente el PC con el parametro `PROGADDR_RESET` (`0x0`) y comienza a ejecutar. Por eso, en `0x0` debe haber una **instruccion de salto** (`j bl_reset_handler`) y no un dato, ya que la CPU decodifica esa primera posicion como codigo ejecutable

- **Cortex-M:** El hardware no ejecuta codigo en `0x0`, sino que lee automaticamente la tabla de vectores: toma la primera palabra como el valor inicial del SP y la segunda como la direccion del Reset Handler. Son **punteros/datos** interpretados

De esta forma, el firmware adopta un esquema de datos similar al de Cortex-M: al inicio de su imagen en memoria (seccion `.fw_header` en `0x1000`) no coloca instrucciones ejecutables, sino una cabecera con metadatos que incluye el puntero de pila inicial (`initial_sp`) y la direccion de su codigo de arranque (`reset_handler`). La diferencia fundamental es que la PicoRV32 desconoce por completo esta estructura a nivel hardware. Es el Bootloader quien, tras validar la integridad de la imagen, mapea dicha cabecera como una estructura en C, extrae ambos valores y ejecuta una rutina en ensamblador que adopta el nuevo stack (`mv sp, a0`) y transfiere el control hacia el Reset Handler mediante un salto (`jr a1`)

### 2. Sobre el Makefile

El Makefile orquesta el proceso para transformar el codigo fuente en una imagen final ejecutable por el simulador:

1. Invoca a GCC dos veces con linker scripts distintos (`bl.ld` y `app.ld`) para producir `bl.elf` (Bootloader) y `app.elf` (Firmware).
2. Con `objcopy -O binary` descarta metadatos, simbolos y secciones de depuracion, extrayendo unicamente los bytes utiles de codigo y datos en `bl.bin` y `app.bin`.
3. **Con `mkimage.py` hace el empaquetado:** Fusiona ambos binarios en un unico archivo `fw.bin`. Rellena el espacio intermedio con ceros para alinear el firmware en `0x1000`, calcula el CRC-8 de la aplicacion y lo escribe en el header.
4. **Formateo para el testbench:** Mediante `hexdump`, traduce `fw.bin` al archivo de texto `fw.hex`, que es el formato que lee el testbench de Verilog con `init_memory()`
5. **Ejecucion y verificacion:** `make sim` corre Verilator capturando el log, y `make check` audita que lo cargado en memoria coincida exactamente con el binario generado.

#### ¿Por que compilar `bl.elf` y `app.elf` por separado?

Porque representan dos entidades de software totalmente independientes con roles, ciclos de vida y layouts de memoria distintos:

- El Bootloader reside en `BL_ROM` (`0x0000`) y tiene prohibido el uso de variables globales en RAM
- El Firmware reside en `APP_ROM` (`0x1000`) y gestiona sus propias secciones `.data` y `.bss` en RAM (`0x20000`)

Enlazarlos por separado garantiza el aislamiento: evita que el linker resuelva simbolos cruzados accidentalmente (como que el BL llame funciones privadas del FW o comparta codigo sin querer). El unico canal de comunicacion entre ambos es el contrato formal definido por la cabecera (`fw_header_t`) y los registros al momento del salto (`a0` y `a1`). Ademas, en un entorno real, esto permite actualizar el firmware sin necesidad de recompilar ni tocar el bootloader pregrabado.

#### ¿Que direccion usa `objcopy -O binary` para `.data`?

Utiliza la **LMA** (Load Memory Address, direccion de carga en ROM) y no la **VMA** (Virtual Memory Address, direccion de enlace en RAM).

En sistemas embebidos, los valores iniciales de las variables globales deben almacenarse en memoria no volatil (ROM). Si `objcopy` utilizara la VMA (`0x00020000`), interpretaria que debe generar un archivo con un hueco gigantesco de ceros entre el fin del codigo en ROM (`0x145B`) y la RAM (`0x20000`), creando un binario innecesariamente enorme. Al usar la LMA, `objcopy` empaqueta los datos iniciales pegados inmediatamente despues del codigo dentro de la imagen ROM. Luego, es tarea del `Reset_Handler` copiar esos bytes desde la ROM a su ubicacion final en RAM antes de entrar a `main`.

#### ¿Que agrega `mkimage.py` entre el BL y el FW?

Agrega un relleno de ceros **(padding)** desde el final del binario del Bootloader (alrededor de `0x03F8`) hasta el inicio del slot del Firmware en `0x1000`.

Este relleno cumple dos propositos fundamentales:

1. **Alineacion espacial:** Asegura que el header y el codigo del firmware queden posicionados exactamente en la direccion `0x1000`, que es donde el Bootloader esta programado para buscarlos.
2. **Proteccion defensiva:** En la arquitectura RISC-V, una palabra compuesta por 32 ceros (`0x00000000`) corresponde a una instruccion ilegal. Si por una falla o bug el Bootloader no ejecutara el salto y se desbordara secuencialmente mas alla de su codigo, la CPU intentaria decodificar estos ceros y entraria en excepcion inmediata (`trap`), previniendo un comportamiento incorrecto.

### 3. Diferencia en la palabra 0x100C

La direccion `0x100C` corresponde al campo `image_crc` dentro de la cabecera del Firmware (`0x1000` + offset `0x0C`), y la discrepancia observada entre ambos archivos es intencional por la etapa en la que interviene cada herramienta:

1. **En `app.elf` (fase de compilacion y enlace):** El compilador y el linker procesan el codigo fuente pero aun no conocen el contenido binario final ni el tamano exacto empaquetado. Por eso, en `app_startup.S` se inicializa este campo con el marcador provisional `FW_IMAGE_CRC_UNSET = 0xFFFFFFFF`. Este valor sirve como un placeholder que indica que la imagen todavia no fue firmada (ningun CRC-8 valido produciria ese numero)

2. **En `fw.hex` (fase de post-procesamiento y empaquetado):** El calculo del CRC ocurre fuera del compilador, ya que el script `tools/mkimage.py` toma el binario crudo `app.bin`, calcula el CRC-8 cubriendo desde la tabla de vectores (`0x10`) hasta el final de la imagen (`image_size`), y sobrescribe directamente esa palabra con el resultado:
   - En la imagen final de produccion (`test_fw_status`): se grabo `0x000000F7`
   - En la variante de prueba (`test_data_bss`): se grabo `0x00000097`

De esta forma, `mkimage.py` modifica unicamente los bytes del binario combinado (`fw.bin`) y no altera el archivo `app.elf`, razon por la cual este ultimo conserva intacto el `0xFFFFFFFF`

### 4. Comparar el FW anterior y el relinkeado

Al comparar el desensamblado del firmware original (`standalone.elf` en `0x0`) con el del firmware relinkeado (`app.elf` en `0x1000`), se observa que instrucciones son independientes de la posicion en memoria y cuales se ven afectadas por el cambio de direccion:

#### Instrucciones que NO cambian

- **Operaciones logicas y aritmeticas sobre registros:** Las instrucciones que operan internamente en la CPU (como `lbu a5, 0(a0)`, `add`, `addi sp, sp, -32`) conservan exactamente la misma codificacion hexadecimal. No dependen de la direccion absoluta en la que esten alojadas
- **Saltos relativos (branch y jump):** En RISC-V, los saltos incondicionales (`j / jal`) y condicionales (`beq`, `bnez`) codifican un desplazamiento relativo al Program Counter. Si tanto el origen del salto como su etiqueta de destino pertenecen a la misma funcion y se trasladaron en bloque hacia `0x1000`, la distancia en bytes entre ambos permanece inalterada.

#### 2. Instrucciones que SI cambian

**Carga de direcciones absolutas y cadenas:** Cambian todas las instrucciones que necesitan apuntar a objetos con ubicacion fija en memoria, como cadenas de texto en `.rodata` o punteros globales.

*Ejemplo en `main`:* Para cargar la direccion del mensaje inicial `"-- running test_fw_status --"`:

- En standalone, la cadena residia en la direccion baja `0x03A0`. Como ese valor cabe en 12 bits con signo, el compilador lo resolvio con una sola instruccion: `li a0, 928` (`3a000513`)

- Al relinkear el firmware a partir de `0x1000`, la misma cadena quedo desplazada a `0x1428`. Al superar el limite de 12 bits, el compilador debio sintetizar la direccion utilizando **dos instrucciones**: `lui a0, 0x1` seguida de `addi a0, a0, 1064`.

#### Conclusion

Relinkear el firmware a `0x1000` no produce una simple traslacion aritmetica de bytes en el archivo `.hex`. Ademas de que el nuevo firmware incluye 24 bytes de encabezado (`.fw_header`) y una rutina de arranque mas extensa (`Reset_Handler` con bucles de inicializacion), el desplazamiento de direcciones absolutas obliga al compilador a generar secuencias de instrucciones distintas (`lui` + `addi`), alterando el tamano y la distribucion del codigo maquina resultante

### 5. jr frente a jalr ra y recarga de stack

#### ¿Por que el salto se realiza con `jr` y no con `jalr ra`?

La diferencia fundamental esta en si queremos que el procesador guarde o no una direccion de retorno:

- **Si usaramos `jalr ra, 0(a1)`:** Esta instruccion guarda automaticamente en el registro `ra` la direccion siguiente del Bootloader. Eso convertiria al Firmware en una "subrutina" que espera volver. Pero el Firmware es la aplicacion final del sistema, no una funcion subordinada: no tiene que retornar jamas al Bootloader. Si dejaramos esa direccion en `ra` y por cualquier error el Firmware ejecutara una instruccion de retorno, la CPU volveria a caer dentro del codigo del Bootloader, pero con la pila ya cambiada y el contexto destruido, provocando una corrupcion de memoria o un cuelgue impredecible.

- **Por que usamos `jr a1`:** En RISC-V, `jr a1` es equivalente `jalr x0, 0(a1)`. Al indicar como destino el registro `x0` (cableado permanentemente a cero), el hardware descarta fisicamente la direccion de retorno. Comunica de forma explicita que es un salto definitivo y sin regreso. Ademas, justo antes del salto ejecutamos `li ra, 0` para limpiar cualquier retorno viejo que haya quedado en `ra`: si el Firmware alguna vez intentara volver, saltara a la direccion `0x0`, provocando un reinicio limpio o un fallo controlado en vez de ejecutar basura

#### ¿Por que el FW vuelve a cargar `sp` si el BL ya lo configuro?

En `app_startup.S`, la primera instruccion de `Reset_Handler` es `la sp, _stack_top`, repitiendo el valor que el Bootloader acaba de colocar en `sp`. Esto responde a principios fundamentales de diseno de software embebido:

1. **Principio de autonomia y robustez:** El Firmware no debe asumir que el entorno previo le dejo un estado de registros perfecto. Para cumplir con el contrato de ejecucion de C, el propio codigo de arranque de la aplicacion tiene la responsabilidad ineludible de garantizar que el puntero de pila sea valido antes de llamar a `main`
2. **Desacoplamiento del loader:** Esta redundancia permite que el Firmware sea portable e independiente del mecanismo de carga. Si en otro escenario el Firmware se grabara directo en hardware o se ejecutara sin Bootloader, funcionara de manera transparente sin depender de inicializaciones externas
3. **Costo despreciable:** Al costar solo una instruccion que se ejecuta una unica vez durante el ciclo de vida del procesador, aporta seguridad y robustez arquitectural sin impacto en memoria ni rendimiento.

### 6. Exito y error

#### ¿Por que el Bootloader no puede escribir `0xFF`?

Porque en el testbench, el valor `0xFF` en `REGISTER_FW_STATUS` representa la finalizacion exitosa de toda la prueba

El Bootloader es un cargador previo: su rol termina cuando valida la imagen y cede el control. Si el Bootloader escribiera `0xFF` al comprobar que el header o el CRC son validos, el testbench interpretaria que el test general concluyo, cortaria la simulacion inmediatamente y el Firmware jamas llegaria a ejecutarse. Por este motivo, el Bootloader mantiene el registro en `0` (o estados intermedios) y le delega exclusivamente a la aplicacion final la responsabilidad de declarar el exito tras completar sus chequeos.

#### ¿Por que ante un error no alcanza con escribir `0xBAD`?

El registro con el valor `0xBAD` sirve como diagnostico para inspeccionar en las ondas (*waveforms*) o en consola que ocurrio, pero no detiene la simulacion.

Al evaluar el bucle `REGISTER_FW_STATUS != 32'hFF`, cualquier valor distinto de `0xFF` (incluido `0xBAD`) hace que el testbench continue ejecutandose. Si el software se limitara a escribir `0xBAD` y quedarse en un bucle infinito, la simulacion ciclaria indefinidamente sin terminar nunca

Para forzar la detencion inmediata por falla, el software debe satisfacer la segunda condicion de salida: `trap == 1`. Por eso, la rutina `bl_fail()` realiza una parada:

1. Escribe `REG_FW_STATUS = 0xBAD` y un codigo descriptivo en `REG_FW_REPORT` para diagnostico
2. Invoca a `bl_halt()`, que ejecuta la instruccion ensamblador `ebreak`.
3. El procesador entra en excepcion y activa su linea fisica de hardware `trap`, senal que el testbench detecta para salir del bucle, imprimir el mensaje de error y cerrar la simulacion.

### 7. Funciones de strutils y ausencia de libc

#### ¿Que funciones de `strutils` quedan en `bl.elf`?

Al ver los simbolos del ejecutable final com `riscv64-unknown-elf-nm -n fw/build/bl.elf`, se observa que la **unica** funcion que permanece en el binario es **`str_crc8`**

Ninguna otra funcion de la biblioteca (como `strbuf_append` o `str_trim`) forma parte del ejecutable final del Bootloader

#### ¿Por que el linker solo incluye `str_crc8`?

Esto es el resultado directo de la combinacion de dos banderas de optimizacion en la cadena de compilacion:

1. **`-ffunction-sections` (en GCC):** Por defecto, el compilador coloca todas las funciones de un archivo fuente en un unico bloque `.text`. Al activar este flag, cada funcion se empaqueta en su propia seccion individual (por ejemplo, `str_crc8` en `.text.str_crc8`)
2. **`-Wl,--gc-sections` (en el Linker `ld`):** Activa la eliminacion de codigo muerto (*Garbage Collection* de secciones). El linker construye el arbol de llamadas comenzando desde el punto de entrada (`_bl_vectors` / `bl_main`). Cualquier seccion a la que nadie llame se descarta fisicamente del binario final.

Dado que `bl_main()` solo necesita validar la integridad de la imagen mediante `str_crc8()`, el linker descarta cualquier otra rutina no referenciada, logrando que el binario del Bootloader sea ultracompacto (apenas 1016 bytes).

### 8. Compresion de instrucciones

En la configuracion del procesador (`rtl/top.v`), las direcciones fijas de entrada por hardware son inmutables:

- Reset fisico: `PROGADDR_RESET = 0x00000000`
- Interrupcion fisica (IRQ): `PROGADDR_IRQ = 0x00000010` (offset de 16 bytes)

Para cumplir con este requisito, la tabla de vectores en `bl_start.S` se diseno disponiendo 5 saltos consecutivos:

- Direccion `0x00`: `j bl_reset_handler`
- Direccion `0x04`: `j bl_unexpected`
- Direccion `0x08`: `j bl_unexpected`
- Direccion `0x0C`: `j bl_unexpected`
- Direccion `0x10`: `j bl_unexpected` (entrada de IRQ)

Esta estructura geometrica supone de forma que cada instruccion `j` ocupa exactamente 4 bytes ($4 \text{ instrucciones} \times 4\text{ bytes} = 16\text{ bytes} = \text{offset } 0x10$)

#### ¿Que pasaria si se compila con `rv32ic` sin `.option norvc`?

La extension `C` (Compressed) de RISC-V incorpora instrucciones de longitud variable de 16 bits (2 bytes). La instruccion de salto incondicional a distancias cortas `j` dispone de su variante comprimida `c.j`.

Si se compilara el Bootloader bajo la arquitectura `rv32ic` sin la proteccion de `.option norvc`:

1. El ensamblador optimizaria automaticamente cada salto reduciendolo a 2 bytes. Como resultado, las cuatro instrucciones iniciales ocuparian solo $4 \times 2 = 8\text{ bytes}$, provocando que el salto asignado a la IRQ quede ubicado en la direccion `0x00000008` en lugar de `0x00000010`.
2. En la direccion fija `0x00000010` ya no existiria el vector de interrupcion, sino codigo arbitrario perteneciente a `bl_reset_handler` o a otra funcion subsiguiente. Ante la llegada de una interrupcion, el hardware forzaria el Program Counter ($PC$) ciegamente a `0x10`, ejecutando instrucciones descontextualizadas a mitad de una rutina ajena, desalineando el flujo de la CPU y colgando irremediablemente el microcontrolador.

#### Solucion implementada

La directiva `.option norvc` en `bl_start.S` (encapsulada entre `.option push` y `.option pop`) le ordena de manera explicita al ensamblador que dentro de ese bloque **suspenda la compresion y fuerce todas las instrucciones al ancho estandar de 32 bits (4 bytes)**. De esta forma, se blinda el espaciado geometrico de la tabla garantizando que el vector de IRQ caiga siempre en `0x10`, independientemente de los flags de optimizacion o de arquitectura que se utilicen al compilar.
