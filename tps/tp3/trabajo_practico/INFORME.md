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
