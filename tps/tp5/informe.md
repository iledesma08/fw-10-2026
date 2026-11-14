# TP5

## Hito 1: Driver de LED

Si idx no es válido (no cumple `idx >= 0 && idx < 8`), las funciones de escritura descartan la operación sin alterar el registro, y funciones de lectura como `led_get_status` retornan 0.

Use esta politica y no otra por varias razones:

- Como las funciones de escritura no pueden devolver nada, no puedo hacer que devuelvan un codigo de error o -1 por ejemplo
- Inducir un crash o disparar una excepcion me parece exagerado, encender el LED no es una tarea critica y no poder encender un LED inexistente no me parece algo suficientemente importante como para detener el sistema.
- Usar el modulo `idx % 8` para que, por ejemplo, al pasar idx = 8 incumple la consigna de no modificar el registro

## Hito 2: Drivers de UART

### UART0

#### 1. Diseño y Reutilización de Código (Principio DRY)

Tanto el envío de un único carácter (`uart_write_byte`) como el de una cadena (`uart_write_str`) requieren interactuar con el periférico siguiendo la misma secuencia de hardware: esperar a que no haya transmisiones pendientes, cargar el buffer de salida, configurar la longitud del bloque, emitir el pulso de disparo y esperar la finalización.

Para evitar duplicar código, se encapsuló esta secuencia en una función estática (para que sea privada y solamente pueda ser accedida por este archivo):

```c
static void uart_send_buffer(const char *buf, unsigned int len);
```

De esta manera:

- `uart_write_byte` reutiliza la función tratándolo como un buffer de longitud 1.
- `uart_write_str` calcula la longitud del string (acotada al tamaño máximo `REG_ARRAY_UART0_BUFF_OUT_SIZE` para prevenir desbordamientos) y delega el envío en `uart_send_buffer`.

#### 2. Test

Hice un test en `fw/tests/test_uart0.c` que valida:

1. Transmisión de un byte individual (`uart_write_byte`).
2. Transmisión de una cadena completa (`uart_write_str`).
3. Manejo de cadena vacía.
4. Verificación directa de los registros de hardware (`REG_UART0_SIZE_TO_TRANSMIT`, contenido de `REG_ARRAY_UART0_BUFF_OUT` y `REG_UART0_TRANSMISION_DONE`).

Comandos utilizados para compilar y ejecutar la simulación:

```bash
make -C fw TARGET=test_uart0

make sim
```

Resultado obtenido: **`RESULT: PASS`** (con `REG_FW_STATUS = 0xFF`).

### UART1

## Hito 3: Driver de DMA

## Hito 4: Driver de ADC
