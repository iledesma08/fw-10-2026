# TP5

## Hito 1: Driver de LED

#### 1. Diseño, Encapsulamiento y Manipulación de Bits

El periférico de LEDs cuenta con un único registro de hardware de 32 bits (`REG_LED` en `0x4_0038`) del cual solo los 8 bits menos significativos están conectados. El diseño del driver se basó en los siguientes principios:

- **Encapsulamiento del hardware:** El acceso directo a `REG_LED` queda restringido exclusivamente a `fw/drivers/src/led.c`. La aplicación y otros módulos solo pueden interactuar mediante la API pública de `fw/drivers/inc/led.h`.
- **Modificación selectiva de bits (Read-Modify-Write):** Para operar sobre un LED individual sin alterar el estado de los demás, se emplean máscaras de desplazamiento `(1u << idx)`:
  - `led_on`: `REG_LED |= (1u << idx);` para encender (SET).
  - `led_off`: `REG_LED &= ~(1u << idx);` para apagar (CLEAR).
  - `led_toggle`: `REG_LED ^= (1u << idx);` para conmutar con una única operación XOR.
  - `led_get_status`: `(REG_LED >> idx) & 1;` para extraer el bit como booleano (`0` o `1`).
- **Política para índices fuera de rango (`idx >= 8`):**
  Como el hardware solo posee 8 LEDs (índices `0` a `7`), cualquier índice `idx >= 8` se considera inválido:
  - Las funciones de escritura (`led_on`, `led_off`, `led_toggle`) descartan la operación de inmediato sin alterar el registro.
  - La función de lectura (`led_get_status`) retorna `0`.
  - **Justificación:**
    1. Las funciones de escritura tienen firma `void`, por lo que no pueden devolver un código de error numérico (como `-1`).
    2. Disparar una excepción o un cuelgue del sistema (`ebreak`/`trap`) resulta desmedido para un periférico visual no crítico.
    3. Aplicar una operación como `idx % 8` encendería un LED erróneo (por ejemplo, índice 8 prendería el LED 0), violando el requerimiento de no alterar el estado ante entradas inválidas.

#### 2. Test

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
