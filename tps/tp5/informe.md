# TP5

## Hito 1: Driver de LED

El periférico de LEDs cuenta con un único registro de hardware de 32 bits (`REG_LED` en `0x4_0038`) del cual solo los 8 bits menos significativos están conectados.

Si idx no es válido (no cumple `idx >= 0 && idx < 8`), las funciones de escritura descartan la operación sin alterar el registro, y funciones de lectura como `led_get_status` retornan 0.

Use esta politica y no otra por varias razones:

- Como las funciones de escritura no pueden devolver nada, no puedo hacer que devuelvan un codigo de error o -1 por ejemplo
- Inducir un crash o disparar una excepcion me parece exagerado, encender el LED no es una tarea critica y no poder encender un LED inexistente no me parece algo suficientemente importante como para detener el sistema.
- Usar el modulo `idx % 8` para que, por ejemplo, al pasar idx = 8 incumple la consigna de no modificar el registro

#### Test

Hice un test en `fw/tests/test_led.c` que valida:

1. Inicialización (`led_init`) comprobando que el registro `REG_LED` y la máscara queden en 0.
2. Encendido individual (`led_on`) y verificación de estado (`led_get_status`) en bits clave (extremos e intermedios).
3. Apagado individual (`led_off`) verificando la puesta a cero del bit correspondiente sin modificar los demás.
4. Conmutación de estado (`led_toggle`) validando tanto la transición `0 -> 1` como `1 -> 0`.
5. Escritura directa (`led_write`) y configuración de máscara completa (`led_set_mask` / `led_get_mask`).
6. Manejo de índices fuera de rango (`idx = 8, 9, 15, 255`): verifica que `REG_LED` conserve exactamente su valor previo y que `led_get_status` devuelva 0.

Comandos utilizados para compilar y ejecutar la simulación:

```bash
make -C fw TARGET=test_led

make sim
```

Resultado obtenido: **`RESULT: PASS`** (con `REG_FW_STATUS = 0xFF`).

## Hito 2: Drivers de UART

### UART0

Tanto el envío de un único carácter (`uart_write_byte`) como el de una cadena (`uart_write_str`) requieren interactuar con el periférico siguiendo la misma secuencia de hardware: esperar a que no haya transmisiones pendientes, cargar el buffer de salida, configurar la longitud del bloque, emitir el pulso de disparo y esperar la finalización.

Para evitar duplicar código, se encapsuló esta secuencia en una función estática (para que sea privada y solamente pueda ser accedida por este archivo):

```c
static void uart_send_buffer(const char *buf, unsigned int len);
```

De esta manera:

- `uart_write_byte` reutiliza la función tratándolo como un buffer de longitud 1.
- `uart_write_str` calcula la longitud del string (acotada al tamaño máximo `REG_ARRAY_UART0_BUFF_OUT_SIZE` para prevenir desbordamientos) y delega el envío en `uart_send_buffer`.

#### Test

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
