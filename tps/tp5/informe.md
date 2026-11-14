# TP5: Drivers de periféricos

## Hito 1: Preparación del proyecto

Se estructuró la capa de drivers separando las interfaces públicas en `fw/drivers/inc/` y las implementaciones privadas en `fw/drivers/src/`:

- `led.h` y `led.c`
- `uart.h` y `uart.c`
- `uart1.h` y `uart1.c`
- `dma.h` y `dma.c`
- `adc.h` y `adc.c`

### ¿Por qué conviene separar interfaz pública en archivos `.h` e implementación en archivos `.c`?

Para mi, principalmente por legibilidad y seguridad. El archivo `.h` actúa como el contrato de la interfaz pública. Lo que consuma al driver solo necesita saber qué servicios ofrece el módulo y cómo invocarlos, abstrayéndose de cómo interactúa internamente con el hardware. Esto mantiene ocultos los detalles de bajo nivel, lo que previene accesos accidentales o indebidos a los registros fuera del driver por ejemplo.

## Hito 2: Driver de LED

El periférico de LEDs cuenta con un único registro de hardware de 32 bits (`REG_LED` en `0x4_0038`) del cual solo los 8 bits menos significativos están conectados.

### 1. Paso a paso de la implementación

- Se implementaron las siguientes funciones:
  - `led_init`: limpia el registro poniendo todos los LEDs en estado apagado.
  - `led_on`: enciende el LED correspondiente al índice idx aplicando una OR: `*reg |= (1 << idx)`.
  - `led_off`: apaga el LED correspondiente al índice idx aplicando una AND con el complemento: `*reg &= ~(1 << idx)`.
  - `led_toggle`: alterna el estado del LED correspondiente al índice idx aplicando una XOR: `*reg ^= (1 << idx)`.
  - `led_write`: escribe un valor de 8 bits en el registro de LEDs.
  - `led_set_mask` / `led_get_mask`: configuración y consulta de la máscara de LEDs.
  - `led_get_status`: retorna el estado actual (`0` o `1`) del LED consultado.

### 2. Decisiones técnicas

Si idx no es válido (no cumple `idx >= 0 && idx < 8`), las funciones de escritura descartan la operación sin alterar el registro, y funciones de lectura como `led_get_status` retornan 0.

Use esta politica y no otra por varias razones:

- Como las funciones de escritura no pueden devolver nada, no puedo hacer que devuelvan un codigo de error o -1 por ejemplo
- Inducir un crash o disparar una excepcion me parece exagerado, encender el LED no es una tarea critica y no poder encender un LED inexistente no me parece algo suficientemente importante como para detener el sistema.
- Usar el modulo `idx % 8` para que, por ejemplo, al pasar idx = 8 incumple la consigna de no modificar el registro

### 3. Errores encontrados y resolución

No hubo errores

### 4. Evidencia de validación

Se implementó el test unitario en `fw/tests/test_led.c`, el cual valida:

1. Inicialización (`led_init`) comprobando que `REG_LED` quede en 0.
2. Encendido individual (`led_on`) y verificación de estado (`led_get_status`) en bits clave.
3. Apagado individual (`led_off`) verificando la puesta a cero del bit sin alterar los restantes.
4. Conmutación de estado (`led_toggle`) en transiciones `0 -> 1` y `1 -> 0`.
5. Escritura directa (`led_write`) y máscaras completas (`led_set_mask` / `led_get_mask`).
6. Manejo de índices fuera de rango (`idx = 8, 9, 15, 255`): verifica que `REG_LED` conserve exactamente su valor previo y que `led_get_status` devuelva 0.

Comandos ejecutados:

```bash
make -C fw TARGET=test_led
make sim
```

Salida de la simulación:

```text
UART: [BL] TP4 bootloader: arranque desde reset (0x00000000)
UART: [BL] buscando FW en 0x00001000
UART: [BL]   magic      = 0x46345054
UART: [BL]   image_size = 0x00000384
UART: [BL]   crc header = 0x0000000D
UART: [BL]   crc calc.  = 0x0000000D
UART: [BL]   initial_sp = 0x00040000
UART: [BL]   reset      = 0x00001018
UART: [BL] FW OK -> salto al Reset_Handler del FW
LED: 0b00000001
LED: 0b10000001
LED: 0b10000000
LED: 0b00000000
LED: 0b00001000
LED: 0b10101010
LED: 0b01010101
LED: 0b00010010
LED: 0b00000000
ending simulation, u_top.u_register_file.REGISTER_FW_STATUS = 000000ff
total clocks:               407250
--- mem report ---
-- total mem access =                 2378
 READ =                 1793, WRITE =                  585
-- total instruction access =                95427
 READ =                95427, WRITE =                    0
RESULT: PASS
- rtl/testbench.sv:123: Verilog $finish
```

## Hito 3: Drivers de UART

### Hito 3.a: Driver UART0

#### 1. Paso a paso de la implementación

UART0 transmite datos organizados en un buffer de tamaño fijo (`REG_ARRAY_UART0_BUFF_OUT_SIZE`). Se realiza la siguiente secuencia:

1. Verificar que no haya transmisiones previas en curso (`REG_UART0_TRANSMISION_DONE == 1`).
2. Copia de los datos al array de salida `REG_ARRAY_UART0_BUFF_OUT`.
3. Configuración del tamaño del bloque a emitir en `REG_UART0_SIZE_TO_TRANSMIT`.
4. Emisión del pulso de inicio escribiendo 1 en `REG_UART0_START_TRANSMISION`.
5. Espera hasta que el periférico confirme el fin de la transmisión (`REG_UART0_TRANSMISION_DONE == 1`).

#### 2. Decisiones técnicas

- Para evitar duplicación de código entre el envío de un carácter individual (`uart_write_byte`) y una cadena (`uart_write_str`), se encapsuló la secuencia completa en una función estática (para que sea privada y solamente pueda ser accedida por este archivo):
  
```c
static void uart_send_buffer(const char *buf, unsigned int len);
```

- `uart_write_byte` delega en `uart_send_buffer` tratándolo como un buffer unitario.
- `uart_write_str` calcula la longitud de la cadena acotándola estrictamente a `REG_ARRAY_UART0_BUFF_OUT_SIZE` para prevenir desbordes de memoria antes de llamar a `uart_send_buffer`.

#### 3. Errores encontrados y resolución

Ninguno

#### 4. Evidencia de validación

Validé que todo andara bien con el test `fw/tests/test_uart0.c`:

1. Transmisión de un byte individual (`uart_write_byte`).
2. Transmisión de una cadena completa (`uart_write_str`).
3. Manejo de cadenas vacías.
4. Verificación de los registros `REG_UART0_SIZE_TO_TRANSMIT`, `REG_ARRAY_UART0_BUFF_OUT` y `REG_UART0_TRANSMISION_DONE`.

Comandos ejecutados:

```bash
make -C fw TARGET=test_uart0
make sim
```

Salida de la simulación:

```text
UART: [BL] TP4 bootloader: arranque desde reset (0x00000000)
UART: [BL] buscando FW en 0x00001000
UART: [BL]   magic      = 0x46345054
UART: [BL]   image_size = 0x00000328
UART: [BL]   crc header = 0x00000079
UART: [BL]   crc calc.  = 0x00000079
UART: [BL]   initial_sp = 0x00040000
UART: [BL]   reset      = 0x00001018
UART: [BL] FW OK -> salto al Reset_Handler del FW
UART: K
UART: UART0_OK
UART: 
[TEST UART0]: Todos los checks pasaron con exito!

ending simulation, u_top.u_register_file.REGISTER_FW_STATUS = 000000ff
total clocks:               369140
--- mem report ---
-- total mem access =                 2477
 READ =                 1830, WRITE =                  647
-- total instruction access =                86764
 READ =                86764, WRITE =                    0
RESULT: PASS
- rtl/testbench.sv:123: Verilog $finish
```

### Hito 3.b: Driver UART1 (UART Serial)

A diferencia de UART0 (que opera por bloques en un buffer de memoria), UART1 es una UART serial 8N1 conectada a dos FIFOs independientes de 16 bytes (RX y TX).

#### 1. Paso a paso de la implementación

- `uart1_init(baud_div)`: programa el divisor en `REG_UART1_BAUD_DIV` y resetea los flags de error.
- `uart1_rx_level()`: lee el nivel de ocupación actual del FIFO de recepción desde `REG_UART1_STATUS`.
- `uart1_getc(uint8_t *b)`: si el FIFO RX no está vacío, extrae el byte, lo guarda en `*b` y retorna 1; si está vacío, retorna 0.
- `uart1_putc(uint8_t b)`: espera a que el FIFO TX tenga espacio libre antes de escribir el byte en `REG_UART1_DATA`.
- `uart1_write(buf, n)`: itera sobre el buffer transmitiendo byte por byte mediante `uart1_putc`.
- `uart1_tx_idle()`: consulta el flag de línea transmisora libre (`UART1_ST_TX_IDLE`).
- `uart1_errors()` / `uart1_clear_errors()`: lee y limpia los flags de error (`UART1_ERR_FRAME` y `UART1_ERR_OVERRUN`) escribiendo en `REG_UART1_ERRORS`.

#### 2. Decisiones técnicas

- Se ajustaron los parámetros de temporización del testbench, mas informacion en el siguiente punto.

#### 3. Errores encontrados y resolución

- El archivo de control `fw/tests/test_uart_serial.txt` fijaba un límite de 300000 ciclos de reloj para aguardar el carácter de inicio `'K'`.
- Al correr la imagen integrada, el Bootloader del TP4 imprime diagnósticos y realiza el cálculo de CRC-8 completo de la Flash, consumiendo aproximadamente 350000 ciclos antes de saltar al punto de entrada del firmware. En consecuencia, el testbench de Verilog abortaba antes de que la aplicación iniciara.

**Solución:** Se ajustó el timeout en `fw/tests/test_uart_serial.txt` a 600000 ciclos (`timeout 600000`), otorgando el margen requerido para que el Bootloader verifique la imagen y ceda el control a `test_uart_serial`.

#### 4. Evidencia de validación

Se validó con la suite `fw/tests/test_uart_serial.c` y el script de prueba:
1. Arranque del firmware y emisión del carácter de inicio `'K'`.
2. Eco íntegro de 256 bytes (`0x00` a `0xFF`) en bloques de 64 bytes.
3. Eco de secuencias similares al protocolo del TP.
4. Detección de *framing error* al recibir un byte con bit de stop en 0.
5. Detección de *overrun* al recibir 40 bytes sin lectura por parte del procesador.
6. Limpieza de los flags de error al llamar a `uart1_clear_errors()`.

Comandos ejecutados:

```bash
make -C fw TARGET=test_uart_serial
./obj_dir/Vtestbench +UART1_SCRIPT=fw/tests/test_uart_serial.txt
```

Salida de la simulación:

```text
UART: [BL] TP4 bootloader: arranque desde reset (0x00000000)
UART: [BL] buscando FW en 0x00001000
UART: [BL]   magic      = 0x46345054
UART: [BL]   image_size = 0x000002D4
UART: [BL]   crc header = 0x000000F1
UART: [BL]   crc calc.  = 0x000000F1
UART: [BL]   initial_sp = 0x00040000
UART: [BL]   reset      = 0x00001018
UART: [BL] FW OK -> salto al Reset_Handler del FW
[ OK ] recibido: 4b 
[ OK ] recibido: 32 bytes
[ OK ] recibido: 32 bytes
[ OK ] recibido: 32 bytes
[ OK ] recibido: 32 bytes
[ OK ] recibido: 32 bytes
[ OK ] recibido: 32 bytes
[ OK ] recibido: 32 bytes
[ OK ] recibido: 32 bytes
[ OK ] recibido: a5 5a 00 ff 5a a5 
[ OK ] recibido: 52 01 
[ OK ] recibido: 53 
[ OK ] recibido: 52 02 
[ OK ] recibido: 52 00 
CHECK PASS (14/14)
ending simulation, u_top.u_register_file.REGISTER_FW_STATUS = 00000000
total clocks:               571787
--- mem report ---
-- total mem access =                25089
 READ =                17244, WRITE =                 7845
-- total instruction access =               143207
 READ =               143207, WRITE =                    0
RESULT: PASS
- rtl/testbench.sv:123: Verilog $finish
```

## Hito 4: Driver de DMA

## Hito 5: Driver de ADC

## Hito 6: Integración en app de demo

## Hito 7: Cierre y validación final
