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
| `building_process_demo` | `-O0` | 7200 | 616 | 112 | 7928 |
| `building_process_demo_opt` | `-O2` | 5903 | 616 | 104 | 6623 |
| Diferencia | | -1297 (-18,0 %) | 0 | -8 (-7,1 %) | -1305 (-16,5 %) |

### Por qué cambia `text`

`text` baja 1297 bytes. La única diferencia entre los dos binarios son las flags (`-O0 -g` contra `-O2 -DNDEBUG`): `-g` no cambia el código y `-DNDEBUG` no tiene efecto porque no hay `assert`, así que la baja se explica por las optimizaciones que agrega `-O2`.

Para confirmar qué optimizaciones inciden, compilé con las mismas flags de `make opt` y usé estos reportes:

- con `-fopt-info-inline` se confirma el inlining:

  ```console
  $ for f in src/hal/hal_ring.c src/hal/hal_uart.c src/lib/strutils.c; do
      gcc -std=c11 -O2 -DNDEBUG -ffunction-sections -fdata-sections \
        -Isrc/lib -Isrc/hal -Isrc/demo -Isrc/app -Isrc/platform \
        -fopt-info-inline -c "$f" -o "/tmp/opencode/$(basename "$f" .c).o"
    done
  src/hal/hal_ring.c:20:7: optimized:  Inlining ring_is_full/1 into ring_push/3.
  src/hal/hal_ring.c:31:7: optimized:  Inlining ring_is_empty/0 into ring_pop/4.
  src/hal/hal_uart.c:16:15: optimized:  Inlining bit_set/0 into uart_init/1.
  src/hal/hal_uart.c:15:15: optimized:  Inlining bit_set/0 into uart_init/1.
  src/hal/hal_uart.c:14:15: optimized:  Inlining bit_set/0 into uart_init/1.
  src/hal/hal_uart.c:24:14: optimized:  Inlining bit_set/0 into uart_send_byte/2.
  src/hal/hal_uart.c:29:5: optimized:  Inlining uart_send_byte/2 into uart_send/3.
  src/lib/strutils.c:71:10: optimized:  Inlined strbuf_append/10 into strbuf_append_int/5 which now has time 243.072975 and size 40, net change of +9.
  ```

- y con `-fopt-info-optimized` GCC reporta, por ejemplo, que desenrolla y vectoriza un bucle de `demo_embedded`:

  ```console
  $ gcc -std=c11 -O2 -DNDEBUG -ffunction-sections -fdata-sections \
      -Isrc/lib -Isrc/hal -Isrc/demo -Isrc/app -Isrc/platform \
      -fopt-info-optimized -c src/demo/demo_embedded.c -o /tmp/opencode/demo_embedded.o
  src/demo/demo_embedded.c:57:24: optimized: loop with 4 iterations completely unrolled (header execution count 97603128)
  src/demo/demo_embedded.c:56:12: optimized: basic block part vectorized using 16 byte vectors
  ```

El efecto también se ve en los tamaños por función. Con `nm -S --size-sort`:

```bash
nm -S --size-sort build/building_process_demo | grep -E ' (t|T) '
nm -S --size-sort build/building_process_demo_opt | grep -E ' (t|T) '
```

| Función | `-O0` (bytes) | `-O2` (bytes) |
|---------|--------------:|--------------:|
| `demo_embedded` | 792 | 594 |
| `str_trim` | 335 | 240 |
| `demo_str` | 301 | 248 |
| `demo_math` | 251 | 203 |
| `ring_pop` | 120 | 54 |
| `ring_push` | 118 | 54 |
| `math_is_prime` | 186 | 199 |
| `math_factorial` | 67 | 79 |

`demo_embedded` mide `0x318` en `-O0` y `0x252` en `-O2`, que equivalen a 792 y 594 bytes. No todo se achica: `math_is_prime` y `math_factorial` crecen un poco porque `-O2` elige otra secuencia de instrucciones. La reducción total es el balance entre lo que baja y lo que sube.

Además, algunas funciones desaparecen como símbolo independiente: `bit_set`, `ring_is_empty`, `ring_is_full`, `strbuf_append` y `uart_send_byte`. No es que no se usen: el compilador las inlinea en sus llamadores, la versión suelta queda sin referencias y `--gc-sections` descarta su sección. La salida de `-fopt-info-inline` de arriba muestra, por ejemplo, `uart_send_byte` dentro de `uart_send` y `strbuf_append` dentro de `strbuf_append_int`.

### Por qué `data` no cambia y `bss` un poquito

También me llamó la atención que `data` diera igual en los dos casos. Para investigarlo, miré las secciones con `size -A`:

```bash
size -A build/building_process_demo | grep -E '^\.(dynamic|got|data|init_array|fini_array)\s'
size -A build/building_process_demo_opt | grep -E '^\.(dynamic|got|data|init_array|fini_array)\s'
```

| Sección | `-O0` | `-O2` |
|---------|------:|------:|
| `.init_array` | 8 | 8 |
| `.fini_array` | 8 | 8 |
| `.dynamic` | 496 | 496 |
| `.got` | 96 | 96 |
| `.data` | 8 | 8 |
| Total de la columna `data` | 616 | 616 |

La columna `data` de `size` no es la sección `.data`: agrupa las secciones inicializadas y escribibles. En este binario son las cinco de la tabla y suman 616 bytes. Al comparar los dos binarios, las cinco secciones miden lo mismo, así que el total de la columna también queda igual.

`data` y `bss` son las dos zonas de variables globales:

- `.data`: variables globales inicializadas con un valor distinto de cero. Sus valores se guardan en el archivo y se copian a RAM al arrancar.
- `.bss`: variables globales que arrancan en cero (o sin inicializador explícito). No se guardan los ceros en el archivo: solo se reserva el tamaño, y el arranque pone esa zona en cero. Por eso `bss` ocupa RAM pero no flash.

Como esa columna no es solo `.data`, quise ver dónde estaban nuestras variables globales. `size` separa las globales inicializadas en `.data` y las que arrancan en cero en `.bss`; por eso también miré `.bss`. Con `nm -S --size-sort` se ven los símbolos y sus tamaños: los que `nm` marca con `b` o `B` son los que van a `.bss`, y los que marca con `d` o `D` los que van a `.data`.

```bash
nm -S --size-sort build/building_process_demo | grep -E ' [bB] '
nm -S --size-sort build/building_process_demo_opt | grep -E ' [bB] '
```

| Símbolo | `-O0` (bytes) | `-O2` (bytes) |
|---------|--------------:|--------------:|
| `completed.0` | 1 | 1 |
| `timer_ms` | 4 | 4 |
| `sim_uart` | 20 | 20 |
| `sim_ring` | 38 | 38 |
| `stdout` | — | 8 |
| Total de `.bss` | 112 | 104 |

`completed.0` es una variable interna del código de arranque. Las globales que aporta nuestro código son `sim_uart`, `sim_ring` y `timer_ms`, y sus tamaños no cambian entre versiones. En `-O2` además aparece una copia de `stdout` de 8 bytes, que no es nuestra.

Para confirmar que no hay globales nuestras en `.data`, filtré los símbolos de tipo `d`/`D`:

```bash
nm -S --size-sort build/building_process_demo | grep -E ' [dD] '    # sin salida
```

No aparece ninguna, así que nuestras variables globales están en `.bss`, no en `.data`. Por eso la columna `data` no cambia.

El total de `.bss` baja de 112 a 104 bytes. En `-O0`, los símbolos de `.bss` que muestra `nm` suman 63 bytes (1 de `completed.0`, 4 de `timer_ms`, 20 de `sim_uart` y 38 de `sim_ring`), así que el resto hasta 112 es padding o alineación. En `-O2` suman 71 porque se agrega `stdout` (8), pero el total es 104: hay 16 bytes menos de padding que en `-O0`, y por eso el total baja 8 aunque `stdout` sume 8. La baja no viene de que nuestras variables se achiquen: sus tamaños son iguales en ambos binarios.

En conclusión, `data` no cambia porque el programa no tiene variables globales inicializadas propias que puedan cambiar de tamaño y reflejar la optimización; lo que pesa en esa columna son secciones fijas del ejecutable. La baja de `bss` viene del acomodo del linker, no de que las demos usen menos RAM.

> En resumen: `-O2` reduce el código un 18,0 % y el total un 16,5 %, mientras que `data` y `bss` quedan prácticamente iguales porque el programa tiene pocas variables globales. En un firmware embebido esa diferencia se traduce directamente en menos flash ocupada y la RAM no cambia de forma apreciable.

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

o sea:

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
- Los `hal_*` y `str_crc8` salen de `libutils.a`; el linker toma de ahí los objetos que necesita.
- `__stack_chk_fail` no es nuestro: ya aparecía en `print.o` en el hito 1 porque lo mete GCC por el stack protector, y lo resuelve la libc. En el binario final queda como `U __stack_chk_fail@GLIBC_2.4`.

#### Cómo comprobé la tabla

`nm -u` solo me dijo qué símbolos usa `demo_embedded.o`; no me dijo dónde están definidos. Para eso miré los objetos y la biblioteca.

Primero, los `print_*`:

```bash
nm --defined-only build/platform/print.o
nm --defined-only build/platform/host/print_host.o
```

```text
0000000000000000 T print_dec
0000000000000000 T print_hex
0000000000000000 T print_str
```

```text
0000000000000000 T print_chr
0000000000000000 T print_flush_buffer
```

Después, los objetos que hay dentro de `libutils.a`:

```bash
ar t build/libutils.a
```

```text
mathlib.o
strutils.o
hal_uart.o
hal_ring.o
hal_timer.o
```

Y qué símbolos define cada uno:

```bash
nm -A --defined-only build/libutils.a | grep -E ' T (ring_init|ring_push|ring_pop|uart_init|uart_send|timer_init|timer_tick|timer_get_ms|str_crc8)$'
```

```text
build/libutils.a:strutils.o:0000000000000000 T str_crc8
build/libutils.a:hal_uart.o:0000000000000000 T uart_init
build/libutils.a:hal_uart.o:0000000000000000 T uart_send
build/libutils.a:hal_ring.o:0000000000000000 T ring_init
build/libutils.a:hal_ring.o:0000000000000000 T ring_pop
build/libutils.a:hal_ring.o:0000000000000000 T ring_push
build/libutils.a:hal_timer.o:0000000000000000 T timer_get_ms
build/libutils.a:hal_timer.o:0000000000000000 T timer_init
build/libutils.a:hal_timer.o:0000000000000000 T timer_tick
```

Con eso se ve que `print_dec`, `print_hex` y `print_str` están en `print.o`; `print_chr` en `print_host.o`; y que `ring_*`, `uart_*`, `timer_*` y `str_crc8` salen de objetos de `libutils.a`. `mathlib.o` también está en la biblioteca, pero `demo_embedded` no usa esas funciones.

`__stack_chk_fail` es aparte: no lo define el TP. En el binario final queda así:

```bash
nm build/building_process_demo | grep __stack_chk_fail
```

```text
                 U __stack_chk_fail@GLIBC_2.4
```

La `U` y el `@GLIBC_2.4` indican que lo resuelve la libc. Aparece porque GCC agrega la protección de stack (stack protector), no porque lo hayamos escrito nosotros. Esa protección guarda un valor testigo en la pila y, al volver de una función, comprueba que no haya cambiado. Si cambió, llama a `__stack_chk_fail`. En el TP no se hace nada con esa función: si el valor está bien, la llamada no se ejecuta; si no, la libc muestra un mensaje de “stack smashing detected” y aborta el programa. No la llamamos nosotros.

### 3. ¿Cuál es la sección más grande del ejecutable?

En el hito 12 ya vimos que `text` es la columna más grande del resumen de `size`, pero `text` no es una sección: es una suma de varias (`size` sin `-A` muestra `text = 7200`, pero eso no es una sola sección; es la suma de `.text` + `.rodata` + `.init` + `.plt*` + `.eh_frame*`, entre otras). Si se analiza sección por sección con:

```bash
size -A build/building_process_demo
```

la más grande entre las que se cargan en memoria es `.text`:

```text
.text        4306
.eh_frame    1132
.dynamic      496
.rodata       260
```

`.text` son 4306 bytes en el build normal (`-O0 -g`); en el optimizado (`-O2`) baja a 3188. Es el código de la app, la biblioteca y la plataforma.

### 4. Después del hito 11, ¿en qué miembro de `libutils.a` está `math_factorial`?

Está en el miembro `mathlib.o` de `libutils.a`. La pregunta apunta al miembro, no al archivo completo: `libutils.a` es un archivo que contiene varios `.o`, y `mathlib.o` es uno de ellos. Ese miembro viene de `src/lib/mathlib.c`, que está en `LIB_SRCS`; el Makefile lo compila a `build/lib/mathlib.o` y `ar` lo archiva dentro de `libutils.a`.

Esto ya lo teníamos del hito 11, cuando listamos los miembros con `ar t`:

```bash
nm -A build/libutils.a | grep math_factorial
```

```text
build/libutils.a:mathlib.o:0000000000000000 T math_factorial
```

La salida de `nm -A` tiene el formato `archivo:miembro:símbolo`, así que `build/libutils.a:mathlib.o:...` significa que `math_factorial` está definido en `mathlib.o`, dentro de `libutils.a`.

El archivo tiene un miembro por cada `.c` de `LIB_SRCS`: `mathlib.o`, `strutils.o`, `hal_uart.o`, `hal_ring.o` y `hal_timer.o`.

### 5. ¿Por qué la aplicación necesita `-lutils` para enlazar, pero `libutils.a` no necesita a la aplicación?

La dependencia va para un solo lado. La base está en el hito 11: `-lutils` le dice al linker que busque en `libutils.a` los símbolos que quedaron sin resolver, y por eso va después de los `.o` de la app.

- La app (`main.o`, `demo_*.o`) tiene referencias sin definir a `math_*`, `str_*`, `uart_*`, etc. Sin `-lutils`, el linker no sabe dónde están y falla con `undefined reference`.
- `libutils.a` es una colección pasiva de objetos: no es un ejecutable, no tiene `main` y no referencia nada de la app. Si se corre `nm -u build/libutils.a`, los únicos símbolos indefinidos son `__stack_chk_fail` y `__ctype_b_loc`, ambos de libc; no hay ninguna referencia a `main` ni a `demo_*`. `__ctype_b_loc` aparece porque `str_trim` usa `isspace`.

> En resumen, la dependencia va en un solo sentido: la app necesita que `libutils.a` le resuelva símbolos, pero la biblioteca no necesita nada de la app. `libutils.a` se arma con `ar` a partir de sus propios `.o`, y el linker la consulta en el enlace final para completar lo que quedó indefinido.
