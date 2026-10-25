# Trabajo Práctico N°3 — Informe

## Hito 1 — Compilar lo que hay

Al inspeccionar `main.o` con `nm` se ve que `print_str` está referenciada pero no definida:

```text
$ nm build/app/main.o
0000000000000000 T main
                 U print_str
```

A partir de esta salida observamos dos letras: 

- `T` significa que el símbolo está definido en la sección `.text` (código) del objeto y tiene alcance global;
- `U` significa *undefined*, es decir que el objeto usa el símbolo pero no lo define. Por eso decimos que `print_str` está referenciada pero no definida en `main.o`.

### **¿En qué objeto de `build/platform/` está `print_str`?**

Luego de buscar entre los pocos archivos buildeados en este primer hito, encontré que `print_str` está definida en `build/platform/print.o`, generado a partir de `src/platform/print.c`:

```text
$ nm build/platform/print.o
                 U print_chr
0000000000000000 T print_dec
0000000000000000 T print_hex
0000000000000000 T print_str
                 U __stack_chk_fail
```

Con esto vemos que la cadena de resolución, que es el recorrido de objetos mediante el cual el linker va satisfaciendo los símbolos que un objeto referencia y no define, no termina ahí: `print.o` necesita `print_chr`, que está definida en `build/platform/host/print_host.o`, generado a partir de `src/platform/host/print_host.c`:

```text
$ nm build/platform/host/print_host.o
                 U fflush
0000000000000000 T print_chr
0000000000000000 T print_flush_buffer
                 U putchar
                 U stdout
```

Es decir: `main.o` → `print.o` → `print_host.o`.

---

### Dudas que me surgieron mientras hacia este hito:

#### **Por que se enlazan los objetos de `build/platform/` si `SRCS` solo tiene `main.c`?**

El Makefile define `PLATFORM = host` y:

```make
PLATFORM_SRCS = src/platform/print.c $(wildcard src/platform/$(PLATFORM)/*.c)
```

`PLATFORM_SRCS` no forma parte de `SRCS`: es una lista aparte que contiene dos cosas:

- `src/platform/print.c`, con el formato común (`print_str`, `print_dec`, `print_hex`), que imprime carácter por carácter llamando a `print_chr`;
- la implementación concreta de las funciones que dependen del sistema donde corre el programa. La plataforma activa la elige la variable `PLATFORM` (acá `host`), y el wildcard toma todos los `.c` de `src/platform/$(PLATFORM)/`. Con `PLATFORM = host`, ese backend es `src/platform/host/print_host.c`, que implementa `print_chr` sobre stdout con `putchar`.

Esos archivos se compilan en `PLATFORM_OBJS`, y la regla de enlace del target los agrega explícitamente junto a los objetos de `SRCS`:

```make
$(TARGET): $(OBJS) $(PLATFORM_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@
```

Por eso, aunque en este hito `SRCS` solo contenga `src/app/main.c`, el ejecutable se enlaza también con `print.o` y `print_host.o`, y `print_str` queda resuelta.

#### **Por que conviene separar entre `print.c` y `print_host.c`?**

La separación entre `print.c` y `print_host.c` tiene un objetivo de portabilidad. `print.h` define la interfaz (`print_str`, `print_dec`, `print_hex`, `print_chr`); `print.c` implementa el formato sin saber a dónde va la salida; y el backend de la plataforma es la única pieza que sabe cómo escribir un carácter en el destino concreto. Por ejemplo, cuando se llama a `print_dec(42)`, `print.c` convierte el número en los caracteres `'4'` y `'2'` y llama a `print_chr` con cada uno; en la plataforma `host`, `print_chr` hace `putchar`, pero en otra plataforma podría escribir en un registro de la UART.

En este repositorio hoy solo existe el backend `host`. La diferenciación está pensada para que el mismo código de `app/`, `lib/` y `hal/` pueda compilarse para distintos destinos cambiando únicamente la implementación de `print_chr`:

> Por ejemplo, para una **Placa RISC-V o FPGA** la salida no sería la stdout de mi compu sino la UART del diseño (`rtl/uart.v`). Se agregaría `src/platform/riscv/` con un `print_chr` que escriba en el registro de datos de la UART, y se cambiaría `PLATFORM = riscv`.

En todos los casos, `print.c`, `main.c`, `lib/` y `hal/` quedan iguales: lo único que cambia es la implementación de `print_chr` y `print_flush_buffer`, que es la frontera entre el código portable y el sistema concreto. Por eso el Makefile elige el backend con `PLATFORM` y el wildcard levanta automáticamente los `.c` de esa carpeta. Además, esto evita que `app/`, `lib/` y `hal/` dependan de `printf` o de stdout, que en un sistema embebido pueden no existir o no ser la salida correcta.

En base a esto, armé un cuadro comparativo de las cuatro categorías:

| Variable | Qué es | Cómo se obtiene | Valor en el hito 1 |
|----------|--------|-----------------|--------------------|
| `SRCS` | fuentes de la aplicación que escribimos | se listan a mano en el Makefile | `src/app/main.c` |
| `PLATFORM_SRCS` | fuentes de la capa de plataforma | `src/platform/print.c` + el wildcard de `src/platform/$(PLATFORM)/*.c` | `src/platform/print.c` y `src/platform/host/print_host.c` |
| `OBJS` | objetos que se compilan a partir de `SRCS` | `patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRCS)` | `build/app/main.o` |
| `PLATFORM_OBJS` | objetos que se compilan a partir de `PLATFORM_SRCS` | `patsubst src/%.c,$(BUILD_DIR)/%.o,$(PLATFORM_SRCS)` | `build/platform/print.o` y `build/platform/host/print_host.o` |

En resumen: `SRCS` y `PLATFORM_SRCS` son listas de archivos fuente; `OBJS` y `PLATFORM_OBJS` son las listas de objetos que resultan de compilarlas. La regla de enlace del ejecutable usa `OBJS` y `PLATFORM_OBJS`, por eso los objetos de la plataforma terminan en el binario aunque no estén en `SRCS`.

## Hito 11 — Biblioteca estática

### Qué cambió en el Makefile

Antes (`USE_STATIC_LIB = 0`) todos los `.c` estaban en `SRCS`: se compilaban a `.o` y el linker recibía todos los objetos sueltos en una sola línea.

```make
$(TARGET): $(OBJS) $(PLATFORM_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@
```

Con `SRCS` incluyendo `lib/` y `hal/`, `OBJS` contenía `main.o`, los `demo_*.o`, `mathlib.o`, `strutils.o`, `hal_uart.o`, `hal_ring.o` y `hal_timer.o`; todos terminaban en el ejecutable.

Ahora (`USE_STATIC_LIB = 1`) `lib/` y `hal/` salen de `SRCS` y pasan a `LIB_SRCS`. Sus objetos se archivan:

```make
$(STATIC_LIB): $(LIB_OBJS) | $(BUILD_DIR)
	$(AR) rcs $@ $^
	$(RANLIB) $@
```

y el ejecutable se enlaza contra el archivo en lugar de contra esos objetos:

```make
$(TARGET): $(APP_OBJS) $(PLATFORM_OBJS) $(STATIC_LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $(APP_OBJS) $(PLATFORM_OBJS) -L$(BUILD_DIR) -lutils -o $@
```

`SRCS` queda con `app/` y `demo/`, `LIB_SRCS` con `lib/` y `hal/`, y `USE_STATIC_LIB` en `1`.

Para guardarlo a modo de repaso, cada expresión de Make significa lo siguiente:

| Expresión | Significado |
|-----------|-------------|
| `objetivo: prerequisitos` | regla: para construir `objetivo` hacen falta esos `prerequisitos` |
| `$(NOMBRE)` | expande la variable `NOMBRE` (por ejemplo, `$(CC)` pasa a ser `gcc`) |
| `ifeq ... else ... endif` | condicional que Make evalúa al leer el Makefile |
| `\| $(BUILD_DIR)` | prerequisito order-only: el directorio debe existir, pero que cambie su fecha no fuerza la reconstrucción |
| `$@` | nombre del objetivo de la regla |
| `$^` | todos los prerequisitos normales, separados por espacios (sin los order-only) |
| `$<` | el primer prerequisito normal (se usa en las reglas de compilación) |
| línea con TAB | cada comando de la receta debe empezar con un tabulador |

### Qué es una biblioteca estática (Repaso para mi)

Una biblioteca estática (`.a`) es un archivo contenedor, en formato `ar`, que agrupa varios `.o`. No es un programa ni algo que se cargue en tiempo de ejecución: es una colección de objetos con un índice de símbolos. El `s` de `ar rcs` y `ranlib` generan ese índice.

Al enlazar, el linker recorre los objetos de la aplicación, ve qué símbolos quedan sin definir y busca en el archivo los miembros que los definen. Solo incorpora esos miembros al ejecutable; el resto queda afuera. El código de los miembros usados se copia dentro del binario final, así que en tiempo de ejecución no hace falta tener `libutils.a` al lado del programa. `ar t` lista los miembros y `nm` muestra los símbolos de cada uno.

#### Glosario (repaso de la Clase 3)

En la Clase 3 vimos que binutils incluye `ar` y `ranlib` para crear e indexar archives, y que `nm` lista símbolos. Estos son los términos que aparecen en este hito:

| Término | Qué es / qué hace |
|---------|-------------------|
| `ar` | GNU archiver: crea y modifica archives (`.a`) |
| archive / `.a` | archivo contenedor de varios `.o`; es el formato de una biblioteca estática |
| `ar rcs lib.a a.o b.o` | `r` reemplaza o inserta miembros, `c` crea el archivo si no existe y `s` escribe el índice de símbolos |
| `ranlib lib.a` | genera o actualiza el índice de símbolos del archive; es redundante después de `ar rcs` (la `s` ya lo hace) |
| `ar t lib.a` | lista los miembros del archive (table of contents) |
| `nm` | lista los símbolos de un objeto o archive: `T`/`t` texto (global/local), `D`/`d` data, `B`/`b` bss, `r` solo lectura, `W` weak |
| `-Ldirectorio` | le dice al linker en qué directorio buscar bibliotecas |
| `-lnombre` | le dice al linker que enlace con `libnombre.a` (o `libnombre.so`); acá `-lutils` busca `libutils.a` |
| enlace estático | el código de la biblioteca se copia dentro del ejecutable al enlazar; el binario queda autónomo, sin dependencia en runtime, pero más grande |
| enlace dinámico | la biblioteca (`.so`) se carga en memoria en tiempo de ejecución; el ejecutable es más chico, pero requiere la biblioteca en el sistema destino |
| `--gc-sections` | el linker descarta las secciones de código y datos que nadie referencia |
| `-ffunction-sections -fdata-sections` | compila cada función y cada dato en su propia sección, para que `--gc-sections` pueda eliminarlos por separado |

En firmware embebido casi siempre se usan bibliotecas estáticas, por autonomía y control de memoria: el binario no depende de que la biblioteca esté instalada en la placa.

### Diferencia con la forma anterior

| Aspecto | Antes (`USE_STATIC_LIB = 0`) | Ahora (`USE_STATIC_LIB = 1`) |
|---------|------------------------------|------------------------------|
| Unidad de enlace | todos los `.o` sueltos | `libutils.a` + objetos de `app/`, `demo/` y `platform/` |
| Qué recibe el linker | todos los objetos de `lib/` y `hal/` | el archivo `libutils.a` con `-lutils` |
| Selección de código | se pasan todos los objetos; `--gc-sections` descarta lo no usado | solo se extraen los miembros que resuelven símbolos; después `--gc-sections` descarta lo no usado dentro de ellos |
| Reutilización | otro programa debe volver a compilar y listar los `.c` | otro programa puede enlazar `libutils.a` sin recompilar la librería |
| Dependencia en runtime | ninguna | ninguna (el código se copia al binario) |
| Actualización | se recompila y se reenlaza todo | se recompila el miembro, se rehace el archivo y se reenlaza la app |
| Inspección | `nm` sobre cada `.o` | `ar t` para ver miembros y `nm` para ver símbolos |

### Qué se gana y qué no

Se gana:

- **Modularidad:** la aplicación depende de una API (`mathlib.h`, `strutils.h`, `hal_*.h`), no de la lista de archivos internos de la librería.
- **Reutilización:** la misma `libutils.a` puede enlazarse en otro ejecutable.
- **Enlace selectivo:** el linker solo trae los miembros que hacen falta. Si un programa usa solo `math_gcd`, no necesita arrastrar `hal_uart.o`.
- **Empaquetado:** se distribuye un solo archivo en lugar de muchos `.o`.

Cosas a tener en cuenta:

- **No es una biblioteca compartida:** no hay una sola copia en memoria ni carga dinámica. Cada ejecutable lleva su propia copia del código.
- **Si cambia la librería, hay que reenlazar la aplicación.**
- **El orden importa:** `-lutils` debe ir después de los objetos que usan sus símbolos. Si va antes, el linker no tiene referencias pendientes y no extrae nada.
- **La granularidad es el objeto, no la función:** si se extrae un miembro, se trae completo; `-ffunction-sections` y `--gc-sections` pueden descartar después las funciones no usadas dentro de ese miembro.

En este TP, `ar t build/libutils.a` muestra los cinco miembros (`mathlib.o`, `strutils.o`, `hal_uart.o`, `hal_ring.o` y `hal_timer.o`), y el ejecutable sigue produciendo exactamente la misma salida que antes del cambio.

## Hito 12 — Optimización

### Medición

La versión normal se compila con `make`, que usa `CFLAGS` con `-O0` y `-g`. La optimizada se compila con `make opt`, que usa `OPT_FLAGS` con `-O2 -DNDEBUG`:

| Ejecutable | Optimización | `text` | `data` | `bss` | `dec` |
|------------|--------------|-------:|-------:|------:|------:|
| `building_process_demo` | `-O0` | 6894 | 600 | 112 | 7606 |
| `building_process_demo_opt` | `-O2` | 5457 | 600 | 104 | 6161 |
| Diferencia | | -1437 (-20,8 %) | 0 | -8 (-7,1 %) | -1445 (-19,0 %) |

### Por qué cambia `text`

`text` baja 1437 bytes porque `-O2` aplica optimizaciones que `-O0` no hace: propagación de constantes, eliminación de código muerto, mejor asignación de registros, simplificación de bucles, reemplazo de secuencias de instrucciones por equivalentes más cortas e inlining de funciones dentro de una misma unidad de compilación.

El efecto se ve en funciones concretas. Podemos analizar sus tamaños por función con `nm -S --size-sort`.

```bash
nm -S --size-sort build/building_process_demo | grep -E ' (t|T) '
nm -S --size-sort build/building_process_demo_opt | grep -E ' (t|T) '
```

| Función | `-O0` (bytes) | `-O2` (bytes) |
|---------|--------------:|--------------:|
| `demo_embedded` | 792 | 594 |
| `str_trim` | 395 | 215 |
| `demo_str` | 301 | 248 |
| `demo_math` | 251 | 203 |
| `ring_pop` | 120 | 54 |
| `ring_push` | 118 | 54 |
| `math_is_prime` | 186 | 199 |
| `math_factorial` | 67 | 79 |

`demo_embedded` mide `0x318` en `-O0` y `0x252` en `-O2`, que equivalen a 792 y 594 bytes.

Es importante destacar que no todo se achica: `math_is_prime` y `math_factorial` crecen un poco porque `-O2` elige otra secuencia de instrucciones (por ejemplo, desenrolla o especializa bucles).

De esta forma, **la reducción total es la suma de lo que baja y lo que sube.**

Además, algunas funciones desaparecen como símbolo independiente: `bit_set`, `ring_is_empty`, `ring_is_full`, `strbuf_append` y `uart_send_byte`. No es que no se usen: el compilador las inlinea en sus llamadores, la versión suelta queda sin referencias y `--gc-sections` descarta su sección.

### Por qué `data` no cambia y `bss` un poquito?

`data` y `bss` son las dos zonas de variables globales:

- `.data`: variables globales inicializadas con un valor distinto de cero. Sus valores se guardan en el archivo y se copian a RAM al arrancar.
- `.bss`: variables globales que arrancan en cero (o sin inicializador explícito). No se guardan los ceros en el archivo: solo se reserva el tamaño, y el arranque pone esa zona en cero. Por eso `bss` ocupa RAM pero no flash.

`size` no informa solo la sección `.data`: su columna `data` agrupa todas las secciones inicializadas y escribibles. En este binario son `.dynamic` (496), `.got` (80), `.data` (8), `.init_array` (8) y `.fini_array` (8), que suman los 600 bytes. Se puede comprobar con:

```bash
size -A build/building_process_demo
```

La sección `.data` mide 8 bytes y no viene de las demos: las demos no tienen variables globales que vayan a `.data`. Sus `static const` (`payload`, `msg`) van a `.rodata`, y los buffers grandes (`sb`, `trimmed`, `frame`, `received`) son locales y viven en el stack. Las únicas variables globales de las demos que van a `.bss` son `sim_uart` y `sim_ring`, que no tienen inicializador. Como `-O2` no cambia esas declaraciones, `data` queda igual.

`bss` baja solo 8 bytes (112 → 104). Los tamaños de los símbolos se ven con `nm -S --size-sort` (la segunda columna es el tamaño en hexadecimal):

```bash
nm -S --size-sort build/building_process_demo | grep -E ' [bB] '
```

```text
0000000000005020 0000000000000001 b completed.0
0000000000005088 0000000000000004 b timer_ms
0000000000005030 0000000000000014 b sim_uart
0000000000005060 0000000000000026 b sim_ring
```

`0x14` son 20 bytes de `sim_uart`, `0x26` son 38 de `sim_ring` y `0x4` son 4 de `timer_ms`; `completed.0` es una variable interna del código de arranque. La suma de los símbolos no da exactamente 112 porque el linker agrega padding para alinear cada símbolo. En `-O2` cambia el orden y el padding, y además aparece una copia de `stdout` de 8 bytes que en `-O0` no estaba. El total baja 8 por el padding, no porque las demos usen menos RAM.

> En resumen: `-O2` reduce el código un 20,8 % y el total un 19,0 %, mientras que `data` y `bss` quedan prácticamente iguales porque el programa tiene pocas variables globales. En un firmware embebido esa diferencia se traduce directamente en menos flash ocupada y la RAM no cambia de forma apreciable.

## Hito 14 — Preguntas

### 1. Tres símbolos de `mathlib.o` y su tipo según `nm`

Esto ya lo vi en el hito 1: `T` es un símbolo definido en `.text` con alcance global, y `U` es *undefined*. El glosario del hito 11 agrega que una `t` minúscula sería local. Al correr:

```bash
nm build/lib/mathlib.o
```

aparece:

```text
0000000000000000 T math_factorial
0000000000000000 T math_gcd
0000000000000000 T math_is_prime
0000000000000000 T math_isqrt
```

que podemos resumir como:

| Símbolo | Tipo | Qué significa |
|---------|------|---------------|
| `math_factorial` | `T` | función global definida en `.text` |
| `math_gcd` | `T` | función global definida en `.text` |
| `math_isqrt` | `T` | función global definida en `.text` |

`math_is_prime` también es `T`. Ninguno es `t` porque no son `static`.

### 2. Símbolos que usa `demo_embedded.o` sin definirlos y dónde se resuelven

En el hito 1 seguimos `main.o -> print.o -> print_host.o` y vimos que `U` significa que el objeto usa el símbolo pero no lo define. Ahora vamos con el caso de `demo_embedded.o`:

```bash
nm -u build/demo/demo_embedded.o
```

aparece:

```text
                 U print_chr
                 U print_dec
                 U print_hex
                 U print_str
                 U ring_init
                 U ring_pop
                 U ring_push
                 U __stack_chk_fail
                 U str_crc8
                 U timer_get_ms
                 U timer_init
                 U timer_tick
                 U uart_init
                 U uart_send
```

osea:

| Símbolo | Se resuelve en |
|---------|----------------|
| `print_str`, `print_dec`, `print_hex` | `build/platform/print.o` (ya estaba en el hito 1) |
| `print_chr` | `build/platform/host/print_host.o` (también del hito 1) |
| `ring_init`, `ring_push`, `ring_pop` | `hal_ring.o`, dentro de `build/libutils.a` |
| `uart_init`, `uart_send` | `hal_uart.o`, dentro de `libutils.a` |
| `timer_init`, `timer_tick`, `timer_get_ms` | `hal_timer.o`, dentro de `libutils.a` |
| `str_crc8` | `strutils.o`, dentro de `libutils.a` |
| `__stack_chk_fail` | libc (lo agrega el stack protector) |

- Los `print_*` se enlazan directo porque `print.c` y `print_host.c` están en `PLATFORM_SRCS`, no en la biblioteca.
- Los `hal_*` y `str_crc8` salen de `libutils.a`; el linker extrae solo los miembros que necesita.
- `__stack_chk_fail` ya aparecía en `print.o` en el hito 1, lo mete GCC por el stack protector.

### 3. ¿Cuál es la sección más grande del ejecutable?

En el hito 12 ya vimos que `text` es la columna más grande del resumen de `size`, pero `text` no es una sección: es una suma de varias (`size` sin `-A` muestra `text = 7138`, pero eso no es una sola sección; es la suma de `.text` + `.rodata` + `.init` + `.plt*` + `.eh_frame*`, entre otras). Si se analiza sección por sección con:

```bash
size -A build/building_process_demo
```

la más grande entre las que se cargan en memoria es `.text`:

```text
.text        4366
.eh_frame    1132
.dynamic      496
.rodata       260
```

`.text` son 4366 bytes en el build normal (`-O0 -g`); en el optimizado (`-O2`) baja a 3172. Es el código de la app, la biblioteca y la plataforma.

### 4. Después del hito 11, ¿en qué miembro de `libutils.a` está `math_factorial`?

En `mathlib.o`. Esto ya lo teníamos del hito 11, cuando listamos los miembros con `ar t`:

```bash
nm -A build/libutils.a | grep math_factorial
```

```text
build/libutils.a:mathlib.o:0000000000000000 T math_factorial
```

El archivo tiene un miembro por cada `.c` de `LIB_SRCS`: `mathlib.o`, `strutils.o`, `hal_uart.o`, `hal_ring.o` y `hal_timer.o`.

### 5. ¿Por qué la aplicación necesita `-lutils` para enlazar, pero `libutils.a` no necesita a la aplicación?

La dependencia va para un solo lado. La base está en el hito 11: `-lutils` le dice al linker que busque en `libutils.a` los símbolos que quedaron sin resolver, y por eso va después de los `.o` de la app.

- La app (`main.o`, `demo_*.o`) tiene referencias sin definir a `math_*`, `str_*`, `uart_*`, etc. Sin `-lutils`, el linker no sabe dónde están y falla con `undefined reference`.
- `libutils.a` es una colección pasiva de objetos: no es un ejecutable, no tiene `main` y no referencia nada de la app. Si se corre `nm -u build/libutils.a`, lo único indefinido que aparece es `__stack_chk_fail` de libc; no hay ninguna referencia a `main` ni a `demo_*`.

> Por eso la app necesita a la lib, pero la lib no necesita a la app. La lib se arma con `ar` a partir de sus propios `.o`; recién en el enlace final el linker la usa para completar los símbolos que faltan.
