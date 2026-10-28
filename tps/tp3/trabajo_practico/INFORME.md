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

