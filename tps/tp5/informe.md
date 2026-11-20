# TP5: Drivers de perifericos

**Autor:** Ignacio Ledesma

## Hito 1: Preparacion del proyecto

Se estructuro la capa de drivers separando las interfaces publicas en `fw/drivers/inc/` y las implementaciones privadas en `fw/drivers/src/`:

- `led.h` y `led.c`
- `uart.h` y `uart.c`
- `uart1.h` y `uart1.c`
- `dma.h` y `dma.c`
- `adc.h` y `adc.c`

### Por que conviene separar interfaz publica en archivos `.h` e implementacion en archivos `.c`?

Para mi, principalmente por legibilidad y seguridad. El archivo `.h` actua como el contrato de la interfaz publica. Lo que consuma al driver solo necesita saber que servicios ofrece el modulo y como invocarlos, abstrayendose de como interactua internamente con el hardware. Esto mantiene ocultos los detalles de bajo nivel, lo que, por ejemplo, previene accesos accidentales o indebidos a los registros fuera del driver

## Hito 2: Driver de LED

El periferico de LEDs cuenta con un unico registro de hardware de 32 bits (`REG_LED` en `0x4_0038`) del cual solo los 8 bits menos significativos estan conectados

### 1. Paso a paso de la implementacion

Fui implementando las funciones en el siguiente orden:

- `led_init`: limpia el registro poniendo todos los LEDs en estado apagado
- `led_on`: enciende el LED correspondiente al indice idx aplicando una OR: `*reg |= (1 << idx)`
- `led_off`: apaga el LED correspondiente al indice idx aplicando una AND con el complemento: `*reg &= ~(1 << idx)`
- `led_toggle`: alterna el estado del LED correspondiente al indice idx aplicando una XOR: `*reg ^= (1 << idx)`
- `led_write`: escribe un valor de 8 bits en el registro de LEDs
- `led_set_mask` / `led_get_mask`: configuracion y consulta de la mascara de LEDs
- `led_get_status`: retorna el estado actual (`0` o `1`) del LED consultado

### 2. Decisiones tecnicas

Si idx no es valido (no cumple `idx >= 0 && idx < 8`), las funciones de escritura descartan la operacion sin alterar el registro, y funciones de lectura como `led_get_status` retornan 0

Use esta politica y no otra por varias razones:

- Como las funciones de escritura no pueden devolver nada, no puedo hacer que devuelvan un codigo de error o -1 por ejemplo
- Inducir un crash o disparar una excepcion me parece exagerado, encender el LED no es una tarea critica y no poder encender un LED inexistente no me parece algo suficientemente importante como para detener el sistema
- Usar el modulo `idx % 8` para que, por ejemplo, al pasar idx = 8 se use el LED 0, incumple la consigna de no modificar el registro cuando idx es invalido

### 3. Errores encontrados y resolucion

No hubo errores

### 4. Evidencia de validacion

Se implemento el test unitario en `fw/tests/test_led.c`, el cual valida:

1. Inicializacion (`led_init`) comprobando que `REG_LED` quede en 0
2. Encendido individual (`led_on`) y verificacion de estado (`led_get_status`) en bits clave
3. Apagado individual (`led_off`) verificando la puesta a cero del bit sin alterar los restantes
4. Conmutacion de estado (`led_toggle`) en transiciones `0 -> 1` y `1 -> 0`
5. Escritura directa (`led_write`) y mascaras completas (`led_set_mask` / `led_get_mask`)
6. Manejo de indices fuera de rango (`idx = 8, 9, 15, 255`): verifica que `REG_LED` conserve exactamente su valor previo y que `led_get_status` devuelva 0

Comandos ejecutados:

```bash
make -C fw TARGET=test_led
make sim
```

Salida de la simulacion:

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

UART0 transmite datos organizados en un buffer de tamano fijo (`REG_ARRAY_UART0_BUFF_OUT_SIZE`)

#### 1. Paso a paso de la implementacion

Fui implementando las funciones en el siguiente orden:

- `uart_init()`: deja `REG_UART0_START_TRANSMIT = 0`, activa `REG_UART0_DATA_FLUSH = 1` para limpiar el estado del periferico y espera a que `REG_UART0_TRANSMISION_DONE` sea 1
- `uart_send_buffer(buf, len)` (auxiliar privada `static`): encapsula la secuencia completa de transmision por bloques:
  1. Valida que `buf` no sea `NULL` y que `len > 0`, acotando `len` a `REG_ARRAY_UART0_BUFF_OUT_SIZE` (256)
  2. Espera que no haya transmisiones previas en curso (`REG_UART0_TRANSMISION_DONE == 1`)
  3. Copia los bytes al buffer de salida `REG_ARRAY_UART0_BUFF_OUT`
  4. Configura la cantidad de bytes a emitir en `REG_UART0_SIZE_TO_TRANSMIT`
  5. Genera el pulso de inicio escribiendo 1 y luego 0 en `REG_UART0_START_TRANSMIT`
  6. Espera hasta que el periferico confirme el fin de la transmision (`REG_UART0_TRANSMISION_DONE == 1`)
- `uart_write_byte(c)`: delega en `uart_send_buffer(&c, 1)` para enviar un unico caracter
- `uart_write_str(s)`: verifica que `s` no sea nulo ni vacio, calcula su longitud hasta `\0` acotandola a `REG_ARRAY_UART0_BUFF_OUT_SIZE` y llama a `uart_send_buffer(s, len)`

#### 2. Decisiones tecnicas

- Para evitar duplicacion de codigo entre el envio de un caracter individual (`uart_write_byte`) y una cadena (`uart_write_str`), se encapsulo la secuencia completa en una funcion estatica (para que sea privada y solamente pueda ser accedida por este archivo):
  
```c
static void uart_send_buffer(const char *buf, unsigned int len);
```

- `uart_write_byte` delega en `uart_send_buffer` tratandolo como un buffer unitario
- `uart_write_str` calcula la longitud de la cadena acotandola estrictamente a `REG_ARRAY_UART0_BUFF_OUT_SIZE` para prevenir desbordes de memoria antes de llamar a `uart_send_buffer`

#### 3. Errores encontrados y resolucion

Ninguno

#### 4. Evidencia de validacion

Valide que todo andara bien con el test `fw/tests/test_uart0.c`:

1. Transmision de un byte individual (`uart_write_byte`)
2. Transmision de una cadena completa (`uart_write_str`)
3. Manejo de cadenas vacias
4. Verificacion de los registros `REG_UART0_SIZE_TO_TRANSMIT`, `REG_ARRAY_UART0_BUFF_OUT` y `REG_UART0_TRANSMISION_DONE`

Comandos ejecutados:

```bash
make -C fw TARGET=test_uart0
make sim
```

Salida de la simulacion:

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

A diferencia de UART0 (que opera por bloques en un buffer de memoria), UART1 es una UART serial 8N1 conectada a dos FIFOs independientes de 16 bytes (RX y TX)

#### 1. Paso a paso de la implementacion

Fui implementando las funciones en el siguiente orden:

- `uart1_init(baud_div)`: programa el divisor en `REG_UART1_BAUD_DIV` y resetea los flags de error
- `uart1_rx_level()`: lee el nivel de ocupacion actual del FIFO de recepcion desde `REG_UART1_STATUS`
- `uart1_getc(uint8_t *b)`: si el FIFO RX no esta vacio, extrae el byte, lo guarda en `*b` y retorna 1; si esta vacio, retorna 0
- `uart1_putc(uint8_t b)`: espera a que el FIFO TX tenga espacio libre antes de escribir el byte en `REG_UART1_DATA`
- `uart1_write(buf, n)`: itera sobre el buffer transmitiendo byte por byte mediante `uart1_putc`
- `uart1_tx_idle()`: consulta el flag de linea transmisora libre (`UART1_ST_TX_IDLE`)
- `uart1_errors()` / `uart1_clear_errors()`: lee y limpia los flags de error (`UART1_ERR_FRAME` y `UART1_ERR_OVERRUN`) escribiendo en `REG_UART1_ERRORS`

#### 2. Decisiones tecnicas

- Se ajustaron los parametros de temporizacion del testbench, mas informacion en el siguiente punto

#### 3. Errores encontrados y resolucion

- El archivo de control `fw/tests/test_uart_serial.txt` fijaba un limite de 300000 ciclos de reloj para aguardar el caracter de inicio `'K'`
- Al correr la imagen integrada, el Bootloader del TP4 imprime diagnosticos y realiza el calculo de CRC-8 completo de la Flash, consumiendo aproximadamente 350000 ciclos antes de saltar al punto de entrada del firmware. En consecuencia, el testbench de Verilog abortaba antes de que la aplicacion iniciara

**Solucion:** Se ajusto el timeout en `fw/tests/test_uart_serial.txt` a 600000 ciclos (`timeout 600000`), otorgando el margen requerido para que el Bootloader verifique la imagen y ceda el control a `test_uart_serial`

#### 4. Evidencia de validacion

Se valido con la suite `fw/tests/test_uart_serial.c` y el script de prueba:

1. Arranque del firmware y emision del caracter de inicio `'K'`
2. Eco integro de 256 bytes (`0x00` a `0xFF`) en bloques de 64 bytes
3. Eco de secuencias similares al protocolo del TP
4. Deteccion de *framing error* al recibir un byte con bit de stop en 0
5. Deteccion de *overrun* al recibir 40 bytes sin lectura por parte del procesador
6. Limpieza de los flags de error al llamar a `uart1_clear_errors()`

Comandos ejecutados:

```bash
# solo si no existe obj_dir/Vtestbench (ver "Testbench de simulacion" en el Hito 1)
verilator --binary --trace -Wno-fatal -f filelist.f
make -C fw TARGET=test_uart_serial
./obj_dir/Vtestbench +UART1_SCRIPT=fw/tests/test_uart_serial.txt
```

Salida de la simulacion:

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

El bloque de DMA permite transferir bloques contiguos de palabras de 32 bits (4 bytes) directamente entre direcciones del mapa de memoria sin intervencion continua del procesador, liberando ciclos de CPU

El hardware del DMA (`rtl/dma.v`) opera de forma autonoma: al recibir un pulso en `REG_DMA_START`, una maquina de estados recorre las direcciones leyendo una palabra del origen y escribiendola en el destino, incrementando internamente su contador de direcciones (`address_count`) y marcando `REG_DMA_DONE = 1` al finalizar

### 1. Paso a paso de la implementacion

Fui implementando las funciones en el siguiente orden:

- `dma_init()`: pone `REG_DMA_START` en 0 para garantizar que el DMA quede listo para detectar el proximo flanco ascendente y asi comenzar la copia
- `dma_is_done()`: verifica si la operacion de copia del DMA ha finalizado (1 si la transferencia finalizo o esta inactivo, 0 si esta transfiriendo datos)
- `dma_copy(src, dst, n_words)`:
  1. **Politica de seguridad:** verifica que `n_words > 0`, que ni `src` ni `dst` sean `NULL`, que `src != dst`, que ambas direcciones esten alineadas a 4 bytes (`((uintptr_t)addr & 0x3) == 0`) y, mediante la funcion auxiliar `dma_range_valid`, que todo el rango `[addr, addr + n_words * 4)` caiga dentro del mapa valido (`0x00000000` a `0x0005FFFF`). Si alguna condicion falla, retorna inmediatamente sin modificar el hardware ni la memoria
  2. Escribe las direcciones numericas en `REG_DMA_SOURCE_ADDR` y `REG_DMA_DEST_ADDR`, y la cantidad de palabras en `REG_DMA_SIZE`
  3. Genera un flanco de subida escribiendo 0 y luego 1 en `REG_DMA_START`
  4. Espera activamente con `while (!dma_is_done())` hasta que el periferico complete la rafaga
  5. Vuelve a poner `REG_DMA_START = 0` para dejar la senal lista para la siguiente operacion

### 2. Decisiones tecnicas

#### Pregunta de control: Que pasa si `dst` y `src` se solapan? Como lo documentaron?

En el modulo Verilog `rtl/dma.v`, el hardware recorre las direcciones de memoria de manera estrictamente ascendente (desde el indice 0 hasta `n_words - 1` incrementando `address_count << 2`):

- **Si `dst < src`:** La copia se ejecuta correctamente aun si se solapan, ya que cada palabra de `src` se lee antes de que el puntero de escritura de `dst` alcance esa misma posicion
- **Si `dst > src`:** Al solaparse los rangos, las primeras escrituras en `dst` sobreescribiran las palabras superiores de `src` antes de que el DMA haya tenido oportunidad de leerlas, corrompiendo los datos restantes de la transferencia

**Como se documento:** Se documento en el header publico `fw/drivers/inc/dma.h` mediante una etiqueta `@warning` en el docstring de `dma_copy()`, advirtiendo explicitamente que los buffers no deben solaparse con `dst > src` (comportamiento identico al contrato estandar de `memcpy` en C)

#### Casteo de punteros con `uintptr_t`

Al implementar las validaciones de alineacion, necesite comprobar que los dos bits menos significativos de las direcciones fueran cero (`& 0x3`). Como en C el compilador no permite aplicar operadores a nivel de bits directamente sobre punteros (`void *`), tuve que convertirlos a un tipo entero

Al buscar cual era el tipo correcto para hacer operaciones con direcciones en C, encontre `uintptr_t`, que esta disenado especificamente para alojar punteros sin perdida de informacion. Por coherencia, decidi usar `uintptr_t` tanto para el chequeo de alineacion (`(uintptr_t)src & 0x3`) como para la conversion previa a escribir el registro de 32 bits (`(uint32_t)(uintptr_t)src`), evitando ademas posibles advertencias del compilador de pointer-to-int-cast

#### Modo sincronico (espera activa)

Aunque el bloque de DMA funciona de forma asincronica en hardware, la funcion `dma_copy` espera activamente a que `dma_is_done()` devuelva 1 antes de retornar. Esto se decidio principalmente porque el test de la catedra (`test_drivers.c`) espera una operacion bloqueante: apenas retorna `dma_copy()`, verifica `dma_is_done()` y lee el buffer de destino asumiendo que los datos ya estan disponibles

### 3. Errores encontrados y resolucion

Ninguno

### Modificaciones post-cambio del 19-11

En la version original de la consigna, no se especificaba que direcciones eran invalidas, ni se indicaba el comportamiento del bus fuera del mapa de memoria. Con la actualizacion del 19-11 (nueva regla en `tp5.md` y agregado de `w4()` en `test_drivers.c`), se paso a exigir explicitamente que `dma_copy` rechace transferencias cuyo rango salga de 0x00000000 - 0x0005FFFF

Para adaptar el driver y sus pruebas a este requisito agregado a ultimo momento se realizaron los siguientes cambios:

1. **Validacion de rango en `dma.c`:** se agrego la funcion auxiliar `dma_range_valid(addr, n_words)`, que verifica `DMA_MEM_START <= addr < DMA_MEM_END` (`0x00000000` a `0x00060000`, a partir de `ADDR_START_INST`, `ADDR_START_REG_FILE` y `ADDR_SIZE_REG_FILE` de `fw/config.h`) y controla que `n_words <= ((DMA_MEM_END - addr) >> 2)`, es decir, que la cantidad de palabras a copiar no supere las palabras disponibles desde `addr` hasta el limite del mapa
2. **Ampliacion de `fw/tests/test_dma_driver.c`:** se sumaron casos de prueba con `src` y `dst` fuera del mapa (`0x00100000`) y con una transferencia que inicia dentro del mapa pero cruza el limite superior (`0x0005FFF8` con 4 palabras)

### 4. Evidencia de validacion

Se implemento el test unitario `fw/tests/test_dma_driver.c`, el cual valida:

1. Copia normal de un bloque de 32 palabras entre arreglos en RAM, verificando que los datos coincidan y que las posiciones adyacentes de memoria no hayan sido sobreescritas
2. Politicas defensivas contra argumentos invalidos:
   - `n_words == 0`: confirma que el buffer de destino permanezca intacto
   - Punteros nulos (`src == NULL` y `dst == NULL`): retorna sin modificaciones en memoria
   - Direcciones no alineadas (offsets de 1 y 2 bytes): verifica que la operacion no se realice y cause transferencias erroneas
   - Punteros identicos (`src == dst`): no realiza ninguna operacion redundante
   - Direcciones fuera del mapa (`0x00100000` en origen y destino) y cruce de frontera (`0x0005FFF8` con 4 palabras): verifica que no dispare la transferencia ni altere memoria
3. Multiples copias sucesivas con tamanos variables (de 1 a 16 palabras) para comprobar que el driver maneje transferencias consecutivas sin trabarse

Comandos ejecutados:

```bash
make -C fw TARGET=test_dma_driver
make sim
```

Salida de la simulacion:

```text
UART: [BL] TP4 bootloader: arranque desde reset (0x00000000)
UART: [BL] buscando FW en 0x00001000
UART: [BL]   magic      = 0x46345054
UART: [BL]   image_size = 0x0000078C
UART: [BL]   crc header = 0x00000037
UART: [BL]   crc calc.  = 0x00000037
UART: [BL]   initial_sp = 0x00040000
UART: [BL]   reset      = 0x00001018
UART: [BL] FW OK -> salto al Reset_Handler del FW
UART: [TEST DMA]: Iniciando test del driver de DMA...
UART: [TEST DMA]: Todos los checks pasaron con exito!
ending simulation, u_top.u_register_file.REGISTER_FW_STATUS = 000000ff
total clocks:               875954
--- mem report ---
-- total mem access =                 4660
 READ =                 3455, WRITE =                 1205
-- total instruction access =               204726
 READ =               204726, WRITE =                    0
RESULT: PASS
- rtl/testbench.sv:123: Verilog $finish
```

## Hito 5: Driver de ADC

El bloque de ADC captura lotes de 256 muestras digitales de 32 bits a una frecuencia configurable mediante un divisor de reloj, permitiendo operar en modo de captura unica (single) o continua (circular) y senalizando al software tanto la mitad de la adquisicion (`half_done`) como el fin del lote (`acq_done`)

En hardware (`rtl/adc.sv`), sintetiza y cuantiza internamente una senal analogica de prueba y guarda cada muestra en memoria interna al ritmo del `sample_tick`

Para configurarlo y leer las muestras, el ADC expone tres registros mapeados en memoria (`REG_ADC_START` en `0x0004002C`, `REG_ADC_STATUS` en `0x00040030` y `REG_ADC_CLK_DIVIDER` en `0x00040034`) y un buffer de 256 muestras de 32 bits en `0x00040100` (`REG_ARRAY_ADC_BUFF`)

### 1. Paso a paso de la implementacion

Fui implementando las funciones en el siguiente orden:

- `adc_init(clk_div)`: setea el divisor y deja `REG_ADC_START = 0`
- `adc_start(adc_mode)`: escribe 0 en `REG_ADC_START` y luego hace `adc_mode & 0x3`. El `& 0x3` descarta los bits altos y deja solo los dos controles:
  - Bit 0 dispara la captura poniendo un 1 en `i_start[0]` del RTL. Segun lo que entendi, gracias a esto, el RTL puede detectar un flanco de subida, por eso sin este bit una captura nunca arranca
  - Bit 1 determina el tipo de captura: con 0 la captura es **single** (al completar las 256 muestras el ADC vuelve a IDLE segun la FSM del RTL, se levanta la flag `acq_done` y el buffer conserva los datos hasta la siguiente captura) y con 1 la captura es **circular** (al completar el lote, la FSM comienza otra captura automaticamente)

| `adc_mode` | Bit 1 | Bit 0 | Efecto |
|---|---|---|---|
| `0x0` | 0 | 0 | No arranca |
| `0x1` | 0 | 1 | Single, arranca por flanco |
| `0x2` | 1 | 0 | Circular, arranca por nivel |
| `0x3` | 1 | 1 | Circular, arranca por flanco/nivel |

- `adc_half_done()`: espera de forma bloqueante a que se active el bit 0 de `REG_ADC_STATUS` (`half_done`, leido con mascara `1u`) y devuelve 1
- `adc_done()`: espera de forma bloqueante a que se active el bit 1 de `REG_ADC_STATUS` (`acq_done`, leido con mascara `2u`) y devuelve 1
- `adc_read(data, size)`: valida `data` y `size`, recorta `size` a `REG_ARRAY_ADC_BUFF_SIZE`, dispara una captura nueva con `adc_start(1)` usando el divisor ya configurado, espera con `adc_done()` y copia las muestras al buffer del llamador
  - El flujo de una lectura queda entonces: divisor configurado con `adc_init` -> flanco de arranque -> espera de fin de adquisicion -> copia del buffer

#### De donde salen las muestras?

Mi primera duda al leer el driver fue de donde salian los valores de `REG_ARRAY_ADC_BUFF`. La respuesta esta en `rtl/adc.sv`, que genera internamente una senal senoidal (`gen_signal`), la cuantiza a 32 bits con `quantize` y, en cada `sample_tick`, guarda el codigo en `sample_mem[samp_cnt]`

### 2. Decisiones tecnicas

- **`adc_half_done()` y `adc_done()` bloqueantes:** ambas funciones esperan activamente su flag correspondiente en `REG_ADC_STATUS` usando mascara (`1u` y `2u`) y devuelven 1. En el caso de `adc_done()`, el test oficial llama a la funcion una sola vez tras `adc_start(1)` y enseguida verifica que `done` valga 1, por lo que devolver el estado sin esperar no alcanza
- **Arranque siempre por flanco:** `adc_read()` no depende de `REG_ADC_START` ni del bit done previos, llama siempre a `adc_start(1)`. Asi cada lectura dispara una adquisicion nueva, incluso si el test llama a `adc_read()` varias veces sin reiniciar el periferico
- **Copia acotada:** si `size` supera el tamano del buffer se recorta a `REG_ARRAY_ADC_BUFF_SIZE` (256); si `data` es NULL o `size` es 0, se retorna 0 sin tocar el hardware
- **Respeto del divisor ya configurado:** `adc_read()` adquiere con el valor presente en `REG_ADC_CLK_DIVIDER` sin sobreescribirlo ni fijar un valor por defecto, delegando la configuracion del reloj exclusivamente a `adc_init(clk_div)`

### 3. Errores encontrados y resolucion

#### El test del ADC colgaba la simulacion (version previa al 19-11)

Al simular `test_drivers`, la simulacion no imprimia ningun resultado `Txx` ni `RESULT`, y terminaba recien al agotarse `MAX_CYCLES`

Despues de investigar un poco, me di cuenta que `test_drivers.c` nunca llama a `adc_init()`. El escenario `w3` usa `adc_read()` directamente y recien al final prueba `set_adc_clk_divider()`. Por lo tanto, `REG_ADC_CLK_DIVIDER` conserva el `0` del reset. Al mismo tiempo, en `rtl/adc.sv`, el bloque divisor recarga su contador con `i_clk_divider - 1`; con divisor 0, `div_cnt` arranca en `0xFFFFFFFF` y la primera muestra tarda aprox 2^32 ciclos. Esto hacia que el testbench quedara esperando indefinidamente

Con esto en mente, tome algunas decisiones:

1. **`adc_read()` fuerza un divisor minimo de 1.** Un divisor 1 es el minimo valido del RTL y no altera el resto de la logica

2. **`adc_read()` siempre genera un flanco de arranque.** `w3` llama a `adc_read()` varias veces seguidas sin `adc_start()` entre medio. Mi primera version solo arrancaba si `REG_ADC_START == 0`, despues de la primera lectura el registro quedaba en 1 y las siguientes copiaban el buffer viejo sin adquirir muestras nuevas. Ahora `adc_read()` llama siempre a `adc_start(1)`, que escribe 0 y luego 1 para asegurar el flanco que detecta `rtl/adc.sv`

3. **`adc_done()` bloquea.** En `w3` se hace `adc_start(1)`, se verifica que el bit done este en 0, se llama a `adc_done()` una unica vez e inmediatamente despues se exige que el bit done ya sea 1. Si `adc_done()` solo devolviera el estado actual, el test fallaria cuando la conversion aun no termino. Por eso `adc_done()` espera dentro de la funcion y la consulta no bloqueante se limita a `adc_half_done()`

Con las tres correcciones, la simulacion del test termino con `RESULT: PASS`

### Modificaciones post-cambio del 19-11

La actualizacion de la catedra del 19-11 entro en conflicto directo con tres decisiones de diseno que habia tomado para hacer pasar la version anterior del test, obligando a rehacer parte del driver y sus pruebas:

1. **Eliminacion de `set_adc_clk_divider()` en `adc.h` y `adc.c`:** originalmente agregue esta funcion publica porque el viejo `test_drivers.c` la invocaba en `w3()`. La catedra reemplazo esas llamadas por `adc_init(dv[j])` en `test_drivers.c` y remarco en `tp5.md` que la interfaz debe coincidir exactamente con la requerida, por lo que hubo que borrar la funcion del header y de la implementacion
2. **Eliminacion del fallback `REG_ADC_CLK_DIVIDER == 0 -> 1` en `adc_read()`:** como `test_drivers.c` paso a llamar a `adc_init(3)` al comienzo de `w3()`, desaparecio el bug del divisor en 0 y `tp5.md` paso a exigir explicitamente que `adc_read()` adquiera con el divisor ya configurado y no fije uno propio. Se elimino el `if (REG_ADC_CLK_DIVIDER == 0)` de `adc.c` y se quito el caso de prueba del divisor 0 en `test_adc_driver.c`
3. **Conversion de `adc_half_done()` a funcion bloqueante:** antes habia disenado `adc_half_done()` como una consulta no bloqueante del bit 0 de estado (en contraposicion a `adc_done()` bloqueante). La nueva regla de `tp5.md` establece que tanto `adc_half_done()` como `adc_done()` son bloqueantes y esperan su flag leido con mascara, por lo que se reemplazo `return (REG_ADC_STATUS & 1u) != 0;` por un lazo `while ((REG_ADC_STATUS & 1u) == 0) {}` y se sumo su validacion en `test_adc_driver.c`

### 4. Evidencia de validacion

Se agrego el test `fw/tests/test_adc_driver.c` (sin modificar `fw/tests/test_adc.c`), que valida el chequeo minimo de la consigna:

1. `adc_read(NULL, 8)` y `adc_read(samples, 0)` devuelven 0
2. `adc_init(4)` configura `REG_ADC_CLK_DIVIDER = 4`
3. `adc_read(samples, 256)` devuelve 256, deja el bit done activo y las muestras varian
4. Imprime las 256 muestras por UART0 con `print_dec`
5. Una segunda lectura con `adc_init(8)` vuelve a disparar la adquisicion con el nuevo divisor
6. Con `adc_init(4)` y `adc_start(1)`, `adc_half_done()` bloquea hasta activar el bit 0 (`half_done`) y `adc_done()` bloquea hasta activar el bit 1 (`acq_done`)

Comandos ejecutados:

```bash
# solo si no existe obj_dir/Vtestbench (ver "Testbench de simulacion" en el Hito 1)
verilator --binary --trace -Wno-fatal -f filelist.f
make -C fw TARGET=test_adc_driver
# +NOWAVE y +MAX_CYCLES: ver "Plusargs del testbench" en el Hito 1
./obj_dir/Vtestbench +NOWAVE +MAX_CYCLES=8000000 > fw/build/sim.log 2>&1
make -C fw check
```

Salida (fragmento; las 256 muestras se imprimen completas):

```text
UART: [BL] TP4 bootloader: arranque desde reset (0x00000000)
UART: [BL] buscando FW en 0x00001000
UART: [BL]   magic      = 0x46345054
UART: [BL]   image_size = 0x000009C0
UART: [BL]   crc header = 0x00000040
UART: [BL]   crc calc.  = 0x00000040
UART: [BL]   initial_sp = 0x00040000
UART: [BL]   reset      = 0x00001018
UART: [BL] FW OK -> salto al Reset_Handler del FW
UART: [TEST ADC]: Iniciando test del driver de ADC...
UART: -- ADC data read (div=4) --
UART: 3049610880
UART: 2900250368
UART: 2746612486
UART: 2589570234
...
UART: -- ADC segunda lectura (div=8) OK --
UART: -- ADC half_done y done bloqueantes OK --
UART: [TEST ADC]: Todos los checks pasaron con exito!
ending simulation, u_top.u_register_file.REGISTER_FW_STATUS = 000000ff
total clocks:              4244455
--- mem report ---
-- total mem access =                24064
 READ =                12338, WRITE =                11726
-- total instruction access =              1160035
 READ =              1160035, WRITE =                    0
RESULT: PASS
```

## Hito 6: Integracion en app de demo

### 1. Paso a paso de la implementacion

La demo quedo en `fw/app/main.c`, que es la aplicacion por defecto del proyecto (`TARGET=main`)

El flujo de `main()` es:

1. `uart_init()` y mensajes de inicio por UART0
2. **Etapa 1:** `led_init()`, `led_on(0)` como LED de inicio, `dma_init()` y `adc_init(4)`. Al terminar enciende el LED 1 y verifica `check_led_mask(0x03)`
3. **Etapa 2:** `led_on(2)` como LED de inicio. Arma `src[4]` con valores y `dst[4]` en cero, llama a `dma_copy(src, dst, 4)` y compara palabra por palabra. Al terminar enciende el LED 3 y verifica `check_led_mask(0x0F)`
4. **Etapa 3:** `led_on(4)` como LED de inicio. Llama a `adc_read(samples, 256)`, verifica que devuelva 256 muestras. Imprime las primeras 8 muestras por UART0. La variacion de las muestras ya se valida en el test del ADC (Hito 5); la demo se queda con la adquisicion y la lectura por API publica. Al terminar enciende el LED 5 y verifica `check_led_mask(0x3F)`
5. **Cierre:** `led_write(0xFF)` deja los 8 LEDs encendidos y `check_led_mask(0xFF)` lo verifica
6. Si todo salio bien, deja `REG_FW_STATUS = 0xFF`. En cualquier fallo imprime el error y deja `REG_FW_STATUS = 0xBAD`

El helper `check_led_mask(expected)` imprime la mascara actual con `led_get_mask()` y `print_hex`, la compara contra el valor esperado y devuelve 0 si no coincide. Asi cada etapa valida su par de LEDs sin repetir el codigo de impresion y comparacion

Los numeros se imprimen con `print_dec`/`print_hex`, las mismas utilidades de la biblioteca de plataforma que usa `test_drivers.c`

### 2. Decisiones tecnicas

Ninguna en particular

### 3. Errores encontrados y resolucion

Ninguno

### 4. Evidencia de validacion

Comandos ejecutados (el `verilator` solo hace falta si no existe `obj_dir/Vtestbench`, por ejemplo despues de `make clean`):

```bash
verilator --binary --trace -Wno-fatal -f filelist.f
make -C fw
# +NOWAVE y +MAX_CYCLES: ver "Plusargs del testbench" en el Hito 1
./obj_dir/Vtestbench +NOWAVE +MAX_CYCLES=8000000 > fw/build/sim.log 2>&1
make -C fw check
```

Salida de la demo (fragmento normalizado):

```text
UART: [BL] TP4 bootloader: arranque desde reset (0x00000000)
UART: [BL] buscando FW en 0x00001000
UART: [BL]   magic      = 0x46345054
UART: [BL]   image_size = 0x00000AD4
UART: [BL]   crc header = 0x00000090
UART: [BL]   crc calc.  = 0x00000090
UART: [BL]   initial_sp = 0x00040000
UART: [BL]   reset      = 0x00001018
UART: [BL] FW OK -> salto al Reset_Handler del FW
UART: [Etapa 1] Inicializando drivers...
UART: Drivers inicializados. Etapa 1 completada.
UART: LED mask = 0x03 OK
UART: [Etapa 2] Copia de prueba y validacion de DMA...
UART: Copia de prueba completada. Etapa 2 completada.
UART: LED mask = 0x0F OK
UART: [Etapa 3] Prueba de ADC...
UART: ADC OK. Primeras 8 muestras:
UART: 1445496950
UART: 1294615047
UART: 1148579314
UART: 1008219555
UART: 874333321
UART: 747681382
UART: 628983398
UART: 518913836
UART: LED mask = 0x3F OK
UART: LED mask = 0xFF OK
UART: Demo completa.
ending simulation, u_top.u_register_file.REGISTER_FW_STATUS = 000000ff
total clocks:              1345553
RESULT: PASS
```

La demo completa termino con `RESULT: PASS`, sin lineas `Txx` ni `ERROR UART0`. La secuencia `03 -> 0F -> 3F -> FF` muestra el avance de dos LEDs por etapa y el encendido final de los ocho

## Hito 7: Cierre y validacion final

Luego de aplicar las modificaciones del 19-11 en DMA y ADC, se revalidaron desde cero los dos suites pedidos en el checklist final de la consigna (`test_drivers` y `test_uart_serial`) junto con la verificacion de carga de imagen

### `test_drivers`

#### 1. Compilacion desde cero

```bash
make build_project_config
make clean
verilator --binary --trace -Wno-fatal -f filelist.f
make -C fw TARGET=test_drivers
```

#### 2. Test de la catedra

```bash
./obj_dir/Vtestbench +NOWAVE +MAX_CYCLES=8000000 > fw/build/sim.log 2>&1
```

El test recorre los cuatro drivers: LED (`w0`), DMA (`w1`), UART0 (`w2`), ADC (`w3`) y DMA con direcciones invalidas (`w4`). `test_drivers.c` no imprime un resumen de chequeos aprobados: solo emite un `Txx` cuando uno falla (no se emitio ninguno). El resto de la salida son las cadenas aleatorias con las que prueba la UART y las 256 muestras del ADC

```text
ending simulation, u_top.u_register_file.REGISTER_FW_STATUS = 000000ff
total clocks:              6936543
--- mem report ---
-- total mem access =                68287
 READ =                37337, WRITE =                30950
-- total instruction access =              1821013
 READ =              1821013, WRITE =                    0
RESULT: PASS
```

#### 3. Verificacion de la imagen cargada

```bash
make -C fw check
```

```text
[hex] build/fw.hex: 2603 palabras (0x00000000-0x000028ab)
[log] build/sim.log: init_memory() cargo 2603 palabras en memory_i
[log] OK: lo que cargo el simulador coincide palabra por palabra con fw.hex

Header + vector table del FW en 0x00001000 (linea 1025 de fw.hex):
  +0x00 magic         0x46345054 OK
  +0x04 load_addr     0x00001000 OK
  +0x08 image_size    0x000018ac (6316 B, hasta 0x000028ab) OK
  +0x0C image_crc     0x00000019 (recalculado: 0x00000019) OK
  +0x10 initial_sp    0x00040000 <_stack_top> OK
  +0x14 reset_handler 0x00001018 <Reset_Handler> OK
  Reset_Handler segun nm: 0x00001018 OK

RESULTADO: OK
```

### `test_uart_serial`

```bash
make -C fw TARGET=test_uart_serial
./obj_dir/Vtestbench +UART1_SCRIPT=fw/tests/test_uart_serial.txt > fw/build/sim.log 2>&1
make -C fw check
```

```text
CHECK PASS (14/14)
ending simulation, u_top.u_register_file.REGISTER_FW_STATUS = 00000000
total clocks:               571787
--- mem report ---
-- total mem access =                25089
 READ =                17244, WRITE =                 7845
-- total instruction access =               143207
 READ =               143207, WRITE =                    0
RESULT: PASS
```

Para ambos tests, `make -C fw check` termino con `RESULTADO: OK`, confirmando que el CRC y el `Reset_Handler` de la imagen cargada coinciden con `fw.hex`

## Aclaraciones

### Testbench de simulacion

`obj_dir/Vtestbench` lo genera Verilator. Se construye una vez con:

```bash
verilator --binary --trace -Wno-fatal -f filelist.f
```

`make sim` construye y ejecuta el testbench, por eso los hitos 2, 3.a y 4 no necesitan el paso previo. En cambio, los hitos que llaman directamente a `./obj_dir/Vtestbench` (3.b, 5, 6 y 7) requieren que el binario exista (despues de `make clean` en la raiz hay que regenerarlo)

### Args del testbench

En los ultimos hitos, el testbench se configura con argumentos:

- **Por que `+NOWAVE`.** Por defecto el testbench ejecuta `$dumpfile("wave.vcd")` y `$dumpvars`, que registran todas las senales en cada ciclo. En corridas de millones de ciclos el VCD crece mucho y la simulacion se vuelve mas lenta. En los hitos 5 y 6 la evidencia es la salida de UART0 y el `RESULT`, no la forma de onda, asi que el archivo no aporta nada, por eso no lo genero
- **Por que `+MAX_CYCLES=8000000`.** Sin el arg la simulacion no tiene techo: si el FW se cuelga, el testbench no termina nunca. Con 8M de ciclos hay un timeout, la simulacion termina y reporta `RESULT: TIMEOUT` en lugar de quedar colgada
