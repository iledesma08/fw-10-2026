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
