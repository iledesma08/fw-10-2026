# Trabajo Práctico N°3 — Informe

## Hito 12 — Optimización

La versión normal se compila con `make` (`CFLAGS` con `-O0` y `-g`) y la optimizada con `make opt` (`OPT_FLAGS` con `-O2 -DNDEBUG`).

| Ejecutable | Optimización | `text` | `data` | `bss` | `dec` |
|------------|--------------|-------:|-------:|------:|------:|
| `building_process_demo` | `-O0` | 6894 | 600 | 112 | 7606 |
| `building_process_demo_opt` | `-O2` | 5457 | 600 | 104 | 6161 |
| Diferencia | | -1437 (-20,8 %) | 0 | -8 (-7,1 %) | -1445 (-19,0 %) |

`text` baja 1437 bytes. La única diferencia entre los dos binarios son las flags (`-O0 -g` contra `-O2 -DNDEBUG`), así que la baja se explica por las optimizaciones que agrega `-O2`. Para confirmar qué optimizaciones inciden, compilé con las mismas flags de `make opt` y usé estos reportes:

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
  src/lib/strutils.c:70:10: optimized:  Inlined strbuf_append/7 into strbuf_append_int/3 which now has time 243.072975 and size 40, net change of +9.
  ```

- y con `-fopt-info-optimized` GCC reporta, por ejemplo, que desenrolla y vectoriza un bucle de `demo_embedded`:

  ```console
  $ gcc -std=c11 -O2 -DNDEBUG -ffunction-sections -fdata-sections \
      -Isrc/lib -Isrc/hal -Isrc/demo -Isrc/app -Isrc/platform \
      -fopt-info-optimized -c src/demo/demo_embedded.c -o /tmp/opencode/demo_embedded.o
  src/demo/demo_embedded.c:57:24: optimized: loop with 4 iterations completely unrolled (header execution count 97603128)
  src/demo/demo_embedded.c:56:12: optimized: basic block part vectorized using 16 byte vectors
  ```

Por otro lado, con `nm -S --size-sort` obtengo los tamaños por función:

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

> Me surgió la duda de por qué `bit_set`, `ring_is_empty`, `ring_is_full`, `strbuf_append` y `uart_send_byte` no aparecen como símbolos independientes en `-O2`. La razón es que GCC las inlinea en sus llamadores; al quedar sin referencias, `--gc-sections` elimina la copia suelta.

También me llamó la atención que `data` diera igual en los dos casos. Para investigarlo, miré las secciones con `size -A` y vi que la columna `data` de `size` es la suma de varias secciones: `.dynamic`, `.got`, `.data`, `.init_array` y `.fini_array`. Al comparar los dos binarios, esas cinco secciones miden lo mismo, así que el total de la columna también queda igual.

Como esa columna no es solo `.data`, quise ver dónde estaban nuestras variables globales. `size` separa las globales en dos lugares: las inicializadas van a `.data` y las que arrancan en cero a `.bss`; por eso también miré `.bss`. Con `nm -S --size-sort` se ven los símbolos y sus tamaños: los que `nm` marca con `b` o `B` son los que van a `.bss`, y los que marca con `d` o `D` los que van a `.data`. Filtrando `b`/`B` aparecen `sim_uart` (20 bytes), `sim_ring` (38) y `timer_ms` (4), y no aparece ninguna global de tipo `d`/`D`. Eso muestra que nuestras variables globales están en `.bss`, no en `.data`.

El total de `.bss` baja de 112 a 104 bytes al usar `O2`, pero los tamaños de `sim_uart`, `sim_ring` y `timer_ms` son los mismos en ambos binarios. Si nuestras variables no se achican y el total sí baja, la baja no viene de ellas: viene de cómo el linker acomoda el resto de `.bss`, no de que las demos usen menos RAM por ejemplo.

En conclusión, yo creo que `data` no cambia porque el programa no tiene variables globales inicializadas propias que puedan cambiar de tamaño y reflejar la optimización; lo que pesa en esa columna son secciones fijas del ejecutable.

## Hito 14 — Preguntas

### 1. Tres símbolos de `mathlib.o` y su tipo según `nm`

`nm` usa una letra para indicar el tipo:

- `T` mayúscula es un símbolo definido en `.text` y global
- `U` es *undefined*, o sea, el objeto usa el símbolo pero no lo define.
- `t` minúscula sería local.

Al hacer `nm build/lib/mathlib.o` aparece:

```text
0000000000000000 T math_factorial
0000000000000000 T math_gcd
0000000000000000 T math_is_prime
0000000000000000 T math_isqrt
```

Todos estos son funciones globales definidas (por la `T`) en `.text`. Ninguno es `t` porque no son `static`.

### 2. Símbolos que usa `demo_embedded.o` sin definirlos y dónde se resuelven

Al correr `nm -u build/demo/demo_embedded.o` aparece:

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

Se resuelven así:

| Símbolo | Se resuelve en |
|---------|----------------|
| `print_str`, `print_dec`, `print_hex` | `build/platform/print.o` |
| `print_chr` | `build/platform/host/print_host.o` |
| `ring_init`, `ring_push`, `ring_pop` | `hal_ring.o`, dentro de `build/libutils.a` |
| `uart_init`, `uart_send` | `hal_uart.o`, dentro de `libutils.a` |
| `timer_init`, `timer_tick`, `timer_get_ms` | `hal_timer.o`, dentro de `libutils.a` |
| `str_crc8` | `strutils.o`, dentro de `libutils.a` |
| `__stack_chk_fail` | libc (lo agrega el stack protector) |

- Los `print_*` se enlazan directo porque `print.c` y `print_host.c` son objetos de plataforma, no forman parte de `libutils.a`.
- Los `hal_*` y `str_crc8` salen de la biblioteca; el linker toma de ahí los objetos que necesita.
- `__stack_chk_fail` no es nuestro: lo mete GCC por el stack protector y lo resuelve la libc (en el binario final queda como `U __stack_chk_fail@GLIBC_2.4`).

#### Cómo comprobé la tabla

`nm -u` solo me sirvio para ver qué símbolos usa `demo_embedded.o`, no para ver dónde están definidos. Para eso miré los objetos y la biblioteca.

Primero, los `print_*`:

```console
$ nm --defined-only build/platform/print.o
0000000000000000 T print_dec
0000000000000000 T print_hex
0000000000000000 T print_str

$ nm --defined-only build/platform/host/print_host.o
0000000000000000 T print_chr
0000000000000000 T print_flush_buffer
```

Después, los objetos que hay dentro de `libutils.a`:

```console
$ ar t build/libutils.a
mathlib.o
strutils.o
hal_uart.o
hal_ring.o
hal_timer.o
```

Y qué símbolos define cada uno:

```console
$ nm -A --defined-only build/libutils.a | grep -E ' T (ring_init|ring_push|ring_pop|uart_init|uart_send|timer_init|timer_tick|timer_get_ms|str_crc8)$'
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

Con esto se ve que:

- `print_dec`, `print_hex` y `print_str` están en `print.o`; `print_chr` en `print_host.o`
- `ring_*`, `uart_*`, `timer_*` y `str_crc8` salen de objetos de `libutils.a`.
- `mathlib.o` también está en la biblioteca, pero `demo_embedded` no usa esas funciones.

`__stack_chk_fail` es aparte: no lo define el TP. En el binario final queda así:

```console
$ nm build/building_process_demo | grep __stack_chk_fail
U __stack_chk_fail@GLIBC_2.4
```

La `U` y el `@GLIBC_2.4` indican que lo resuelve la libc. Aparece porque GCC agrega la protección de stack (stack protector), no porque lo hayamos escrito nosotros. Esa protección guarda un valor testigo en la pila y, al volver de una función, comprueba que no haya cambiado. Si cambió, llama a `__stack_chk_fail`. En el TP no se hace nada con esa función: si el valor está bien, la llamada no se ejecuta; si no, la libc muestra un mensaje de “stack smashing detected” y aborta el programa.

### 3. ¿Cuál es la sección más grande del ejecutable?

En la tabla del hito 12 se ve que `text` es la columna más grande del resumen de `size`, pero `text` no es una sección: es una suma de varias. Si se mira sección por sección con:

```console
$ size -A build/building_process_demo
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

Ya sabemos que `libutils.a` agrupa varios `.o`, y uno de ellos es `mathlib.o`. Ese miembro sale de `src/lib/mathlib.c`, que está en `LIB_SRCS`; el Makefile lo compila a `build/lib/mathlib.o` y `ar` lo guarda dentro de la biblioteca.

Se ve con:

```console
$ nm -A build/libutils.a | grep math_factorial
build/libutils.a:mathlib.o:0000000000000000 T math_factorial
```

La salida de `nm -A` tiene el formato `archivo:miembro:símbolo`, así que `build/libutils.a:mathlib.o:...` significa que `math_factorial` está definido en `mathlib.o`, dentro de `libutils.a`.

En total, el archivo tiene cinco miembros: `mathlib.o`, `strutils.o`, `hal_uart.o`, `hal_ring.o` y `hal_timer.o`.

### 5. ¿Por qué la aplicación necesita `-lutils` para enlazar, pero `libutils.a` no necesita a la aplicación?

La dependencia va para un solo lado. `-lutils` le dice al linker que busque en `libutils.a` los símbolos que quedaron sin resolver, y por eso va después de los `.o` de la app.

- La app (`main.o`, `demo_*.o`) tiene referencias sin definir a `math_*`, `str_*`, `uart_*`, etc. Sin `-lutils`, el linker no sabe dónde están y falla con `undefined reference`.
- `libutils.a` es una colección pasiva de objetos: no es un ejecutable, no tiene `main` y no referencia nada de la app. Si se corre `nm -u build/libutils.a`, lo único indefinido que aparece es `__stack_chk_fail` de libc, o sea, no hay ninguna referencia a `main` ni a `demo_*`.

En resumen, la dependencia va en un solo sentido: la app necesita que `libutils.a` le resuelva símbolos, pero la biblioteca no necesita nada de la app. `libutils.a` se arma con `ar` a partir de sus propios `.o`, y el linker la consulta en el enlace final para completar lo que quedó indefinido.
