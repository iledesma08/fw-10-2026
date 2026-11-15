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

El bloque de DMA permite transferir bloques contiguos de palabras de 32 bits (4 bytes) directamente entre direcciones del mapa de memoria sin intervención continua del procesador, liberando ciclos de CPU.

### 1. Paso a paso de la implementación

El hardware del DMA (`rtl/dma.v`) opera de forma autónoma: al recibir un pulso en `REG_DMA_START`, una máquina de estados recorre las direcciones leyendo una palabra del origen y escribiéndola en el destino, incrementando internamente su contador de direcciones (`address_count`) y marcando `REG_DMA_DONE = 1` al finalizar.

Las funciones implementadas en `fw/drivers/src/dma.c` son:

- `dma_init()`: pone `REG_DMA_START` en 0 para garantizar que el DMA quede listo para detectar el próximo flanco ascendente y asi comenzar la copia.
- `dma_is_done()`: verifica si la operación de copia del DMA ha finalizado (1 si la transferencia finalizó o está inactivo, 0 si está transfiriendo datos).
- `dma_copy(src, dst, n_words)`:
  - **Validaciones de seguridad (guardas):** verifica que `n_words > 0`, que ni `src` ni `dst` sean `NULL`, que `src != dst`, y que ambas direcciones estén alineadas a 4 bytes (`((uintptr_t)addr & 0x3) == 0`). Si alguna condición falla, retorna inmediatamente sin modificar el hardware ni la memoria.
  1. Escribe las direcciones numéricas en `REG_DMA_SOURCE_ADDR` y `REG_DMA_DEST_ADDR`, y la cantidad de palabras en `REG_DMA_SIZE`.
  2. Genera un flanco de subida escribiendo 0 y luego 1 en `REG_DMA_START`.
  3. Espera activamente con `while (!dma_is_done())` hasta que el periférico complete la ráfaga.
  4. Vuelve a poner `REG_DMA_START = 0` para dejar la señal lista para la siguiente operación.

### 2. Decisiones técnicas

#### Pregunta de control: ¿Qué pasa si `dst` y `src` se solapan? ¿Cómo lo documentaron?

En el módulo Verilog `rtl/dma.v`, el hardware recorre las direcciones de memoria de manera estrictamente ascendente (desde el índice 0 hasta `n_words - 1` incrementando `address_count << 2`):

- **Si `dst < src`:** La copia se ejecuta correctamente aun si se solapan, ya que cada palabra de `src` se lee antes de que el puntero de escritura de `dst` alcance esa misma posición.
- **Si `dst > src`:** Al solaparse los rangos, las primeras escrituras en `dst` sobreescribirán las palabras superiores de `src` antes de que el DMA haya tenido oportunidad de leerlas, corrompiendo los datos restantes de la transferencia.

**Cómo se documentó:** Se documentó en el header público `fw/drivers/inc/dma.h` mediante una etiqueta `@warning` en el docstring de `dma_copy()`, advirtiendo explícitamente que los buffers no deben solaparse con `dst > src` (comportamiento idéntico al contrato estándar de `memcpy` en C).

#### Casteo de punteros con `uintptr_t`

Al implementar las validaciones de alineación, necesité comprobar que los dos bits menos significativos de las direcciones fueran cero (`& 0x3`). Como en C el compilador no permite aplicar operadores a nivel de bits directamente sobre punteros (`void *`), tuve que convertirlos a un tipo entero.

Al buscar cuál era el tipo correcto para hacer operaciones con direcciones en C, encontré `uintptr_t`, que está diseñado específicamente para alojar punteros sin pérdida de información. Por coherencia, decidí usar `uintptr_t` tanto para el chequeo de alineación (`(uintptr_t)src & 0x3`) como para la conversión previa a escribir el registro MMIO de 32 bits (`(uint32_t)(uintptr_t)src`), evitando además posibles advertencias del compilador de pointer-to-int-cast.

#### Modo sincrónico (espera activa)

Aunque el bloque de DMA funciona de forma asincrónica en hardware, la función `dma_copy` espera activamente a que `dma_is_done()` devuelva 1. Esto se decidió para evitar condiciones de carrera en el firmware, garantizando que cuando la función retorne, los datos ya estén disponibles en el buffer de destino para ser consumidos inmediatamente por el código que la invocó.

### 3. Errores encontrados y resolución

**Chequeo prematuro de `dma_is_done()` al inicio:** En una versión preliminar del test, se comprobaba `dma_is_done()` inmediatamente tras `dma_init()`. El testbench abortó con trap porque en la descripción de hardware (`rtl/dma.v`) el registro `dma_done` arranca en 0 en el reset del sistema y solo pasa a 1 tras completar la primera transferencia. Se ajustó el test para validar `dma_is_done()` tras haber disparado transferencias.

### 4. Evidencia de validación

Se implementó el test unitario `fw/tests/test_dma_driver.c`, el cual valida:

1. Copia normal de un bloque de 32 palabras entre arreglos en RAM, verificando que los datos coincidan y que las posiciones adyacentes de memoria no hayan sido sobreescritas.
2. Guardas defensivas contra argumentos inválidos:
   - `n_words == 0`: confirma que el buffer de destino permanezca intacto.
   - Punteros nulos (`src == NULL` y `dst == NULL`): retorna sin modificaciones en memoria.
   - Direcciones no alineadas (offsets de 1 y 2 bytes): verifica que la operación no se realice y cause transferencias erróneas.
   - Punteros idénticos (`src == dst`): no realiza ninguna operación redundante.
3. Múltiples copias sucesivas con tamaños variables (de 1 a 16 palabras) para comprobar que el driver maneje transferencias consecutivas sin trabarse.

Comandos ejecutados:

```bash
make -C fw TARGET=test_dma_driver
make sim
```

Salida de la simulación:

```text
UART: [BL] TP4 bootloader: arranque desde reset (0x00000000)
UART: [BL] buscando FW en 0x00001000
UART: [BL]   magic      = 0x46345054
UART: [BL]   image_size = 0x000006F8
UART: [BL]   crc header = 0x0000003F
UART: [BL]   crc calc.  = 0x0000003F
UART: [BL]   initial_sp = 0x00040000
UART: [BL]   reset      = 0x00001018
UART: [BL] FW OK -> salto al Reset_Handler del FW
UART: [TEST DMA]: Iniciando test del driver de DMA...
UART: [TEST DMA]: Todos los checks pasaron con exito!
ending simulation, u_top.u_register_file.REGISTER_FW_STATUS = 000000ff
total clocks:               810515
--- mem report ---
-- total mem access =                 4506
 READ =                 3304, WRITE =                 1202
-- total instruction access =               189554
 READ =               189554, WRITE =                    0
RESULT: PASS
- rtl/testbench.sv:123: Verilog $finish
```

## Hito 5: Driver de ADC

### 1. Paso a paso de la implementación

El ADC expone tres registros mapeados en memoria (`REG_ADC_START` en `0x0004002C`, `REG_ADC_STATUS` en `0x00040030` y `REG_ADC_CLK_DIVIDER` en `0x00040034`) y un buffer de 256 muestras de 32 bits en `0x00040100` (`REG_ARRAY_ADC_BUFF`).

Las funciones implementadas son:

- `set_adc_clk_divider(clk_div)`: escribe directamente `REG_ADC_CLK_DIVIDER`. Se agregó porque `test_drivers.c` la utiliza, aunque la consigna no la pedía explícitamente.
- `adc_init(clk_div)`: setea el divisor y deja `REG_ADC_START = 0`.
- `adc_start(adc_mode)`: escribe 0 en `REG_ADC_START = 0` y luego hace `adc_mode & 0x3`.El `& 0x3` descarta los bits altos y deja solo los dos controles:
  - Bit 0 dispara la captura poniendo un 1 en `i_start[0]` del RTL. Según lo que entendí, gracias a esto, el RTL puede detectar un flanco de subida, por eso sin este bit una captura nunca arranca
  - Bit 1 determina el tipo de captura: con 0 la captura es **single** (al completar las 256 el ADC vuelve a IDLE segun la FSM del RTL, se levanta la flag `acq_done` y el buffer conserva los datos hasta la siguiente captura) y con 1 la captura es **circular** (al completar el lote, la FSM comienza otra captura automaticamente)

| `adc_mode` | Bit 1 | Bit 0 | Efecto |
|---|---|---|---|
| `0x0` | 0 | 0 | No arranca |
| `0x1` | 0 | 1 | Single, arranca por flanco |
| `0x2` | 1 | 0 | Circular, arranca por nivel |
| `0x3` | 1 | 1 | Circular, arranca por flanco/nivel |

- `adc_half_done()`: devuelve el bit 0 de `REG_ADC_STATUS` (`half_done`) con una lectura no bloqueante.
- `adc_done()`: hace una lectura bloqueante que espera hasta que el adc termina y devuelve 1.
- `adc_read(data, size)`: valida `data` y `size`, recorta `size` a `REG_ARRAY_ADC_BUFF_SIZE`, asegura un divisor distinto de 0, dispara una captura nueva con `adc_start(1)`, espera con `adc_done()` y copia las muestras al buffer del llamador.
  - El flujo de una lectura queda entonces: configuración del divisor → flanco de arranque → espera de fin de adquisición → copia del buffer.

### 2. Decisiones técnicas

- **`adc_done()` bloqueante:** no es un simple getter del bit de estado sino espera a que la conversión termine. El test oficial llama a la funcion una sola vez y enseguida verifica que `done` valga 1, por lo que devolver el estado sin esperar no alcanza, por eso lo hice asi.
- **Divisor mínimo en `adc_read()`:** el test nunca llama a `adc_init()`, así que el registro conserva el 0 del reset. Como el RTL recarga el contador con `divisor - 1`, un 0 produce una espera aproximada de 2^32 ciclos, asi que puse para que, si nunca se configuro el divisor, se configure con 1 que es el minimo.
- **Arranque siempre por flanco:** `adc_read()` no depende de `REG_ADC_START` ni del bit done previos, llama siempre a `adc_start(1)`. Así cada lectura dispara una adquisición nueva, incluso si el test llama a `adc_read()` varias veces sin reiniciar el periférico.
- **Copia acotada:** si `size` supera el tamaño del buffer se recorta a `REG_ARRAY_ADC_BUFF_SIZE` (256); si `data` es NULL o `size` es 0, se retorna 0 sin tocar el hardware.
- **`set_adc_clk_divider()` pública:** se agregó aunque la consigna no la pida porque `test_drivers.c` la invoca.

Estas decisiones están motivadas por el uso concreto que hace `test_drivers.c` (escenario `w3`) y por el comportamiento de `rtl/adc.sv`. El detalle del cuelgue y su diagnóstico está en la sección 3.

### 3. Errores encontrados y resolución

#### El test del ADC colgaba la simulación

Al simular `test_drivers`, la simulación no imprimía ningún resultado `Txx` ni `RESULT`, y terminaba recién al agotarse `MAX_CYCLES`.

Después de investigar un poco, me di cuenta que `test_drivers.c` nunca llama a `adc_init()`. El escenario `w3` usa `adc_read()` directamente y recién al final prueba `set_adc_clk_divider()`. Por lo tanto, `REG_ADC_CLK_DIVIDER` conserva el `0` del reset. Al mismo tiempo, en `rtl/adc.sv`, el bloque divisor recarga su contador con `i_clk_divider - 1`; con divisor 0, `div_cnt` arranca en `0xFFFFFFFF` y la primera muestra tarda aprox 2^32 ciclos. Esto hacia que el testbench quedara esperando indefinidamente.

Con esto en mente, tome algunas decisiones:

1. **`adc_read()` fuerza un divisor mínimo de 1.** Un divisor 1 es el mínimo válido del RTL y no altera el resto de la lógica.

2. **`adc_read()` siempre genera un flanco de arranque.** `w3` llama a `adc_read()` varias veces seguidas sin `adc_start()` entre medio. Mi primera versión solo arrancaba si `REG_ADC_START == 0`, despues de la primera lectura el registro quedaba en 1 y las siguientes copiaban el buffer viejo sin adquirir muestras nuevas. Ahora `adc_read()` llama siempre a `adc_start(1)`, que escribe 0 y luego 1 para asegurar el flanco que detecta `rtl/adc.sv`.

3. **`adc_done()` bloquea.** En `w3` se hace `adc_start(1)`, se verifica que el bit done esté en 0, se llama a `adc_done()` una única vez e inmediatamente después se exige que el bit done ya sea 1. Si `adc_done()` solo devolviera el estado actual, el test fallaría cuando la conversión aún no terminó. Por eso `adc_done()` espera dentro de la función y la consulta no bloqueante se limita a `adc_half_done()`.

Con las tres correcciones, la simulación del test terminó con `RESULT: PASS`.

### 4. Evidencia de validación

Se agregó el test `fw/tests/test_adc_driver.c` (sin modificar `fw/tests/test_adc.c`), que valida el chequeo mínimo de la consigna:

1. `adc_read(NULL, 8)` y `adc_read(samples, 0)` devuelven 0.
2. `adc_init(4)` configura `REG_ADC_CLK_DIVIDER = 4`.
3. `adc_read(samples, 256)` devuelve 256, deja el bit done activo y se hacen variar las muestras.
4. Imprime las 256 muestras por UART0 con `print_dec`.
5. Una segunda lectura con divisor 8 vuelve a disparar la adquisición.
6. Una tercera lectura con divisor 0 fuerza el divisor a 1 y termina sin colgarse.

Comandos ejecutados:

```bash
make -C fw TARGET=test_adc_driver
./obj_dir/Vtestbench +NOWAVE +MAX_CYCLES=8000000 > fw/build/sim.log 2>&1
make -C fw check
```

Salida (fragmento; las 256 muestras se imprimen completas):

```text
UART: [BL] TP4 bootloader: arranque desde reset (0x00000000)
UART: [BL] buscando FW en 0x00001000
UART: [BL]   magic      = 0x46345054
UART: [BL]   image_size = 0x000009A0
UART: [BL]   crc header = 0x00000031
UART: [BL]   crc calc.  = 0x00000031
UART: [BL]   initial_sp = 0x00040000
UART: [BL]   reset      = 0x00001018
UART: [BL] FW OK -> salto al Reset_Handler del FW
UART: [TEST ADC]: Iniciando test del driver de ADC...
UART: -- ADC data read (div=4) --
UART: 60920390
UART: 105105331
UART: 160895474
UART: 227973806
...
UART: -- ADC segunda lectura (div=8) OK --
UART: -- ADC divisor 0 -> 1 OK --
UART: [TEST ADC]: Todos los checks pasaron con exito!
ending simulation, u_top.u_register_file.REGISTER_FW_STATUS = 000000ff
total clocks:              4222939
--- mem report ---
-- total mem access =                24517
 READ =                12619, WRITE =                11898
-- total instruction access =              1154720
 READ =              1154720, WRITE =                    0
RESULT: PASS
```

## Hito 6: Integración en app de demo

## Hito 7: Cierre y validación final
