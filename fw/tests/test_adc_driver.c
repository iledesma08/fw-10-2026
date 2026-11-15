/*
 * Test del driver de ADC para el Hito 5 del TP5
 *
 * Chequeo minimo pedido: obtener una lectura del ADC e imprimirla por UART.
 * Ademas se validan los casos borde del driver y se cubre el divisor 0, que
 * es el que colgaba la simulacion antes de la correccion
 */
#include "adc.h"
#include "includes.h"

static uint32_t samples[REG_ARRAY_ADC_BUFF_SIZE];

/* Mismo criterio que w3a de test_drivers.c: la senal debe variar */
static int samples_vary(const uint32_t *data, uint32_t size) {
  uint32_t different = 0;

  for (uint32_t i = 1; i < size; i++) {
    if (data[i] != data[i - 1]) {
      different++;
    }
  }

  return different >= 64;
}

static void print_samples(const uint32_t *data, uint32_t size) {
  for (uint32_t i = 0; i < size; i++) {
    print_dec(data[i]);
    if ((i & 7u) == 7u) {
      print_chr('\n');
    } else {
      print_chr(' ');
    }
  }
}

static int fail(const char *message) {
  print_str("[TEST ADC]: FAIL: ");
  print_str(message);
  print_str("\n");
  REG_FW_STATUS = 0xBAD;
  return -1;
}

int main(void) {
  uint32_t n;

  REG_FW_STATUS = 0x0;
  REG_FW_REPORT = 0x0;

  print_str("[TEST ADC]: Iniciando test del driver de ADC...\n");

  /* 1. Guardas de argumentos invalidos */
  if (adc_read(NULL, 8) != 0) {
    return fail("adc_read(NULL, 8) debe devolver 0");
  }
  if (adc_read(samples, 0) != 0) {
    return fail("adc_read(samples, 0) debe devolver 0");
  }

  /* 2. Configuracion explicita del divisor */
  adc_init(4);
  if (REG_ADC_CLK_DIVIDER != 4) {
    return fail("adc_init(4) no configuro el divisor");
  }

  /* 3. Lectura completa por la API publica */
  n = adc_read(samples, REG_ARRAY_ADC_BUFF_SIZE);
  if (n != REG_ARRAY_ADC_BUFF_SIZE) {
    return fail("adc_read no devolvio 256 muestras");
  }
  if ((REG_ADC_STATUS & 2u) == 0) {
    return fail("el bit done no quedo activo");
  }
  if (!samples_vary(samples, REG_ARRAY_ADC_BUFF_SIZE)) {
    return fail("las muestras leidas no varian");
  }

  /* 4. Chequeo minimo: imprimir la lectura por UART */
  print_str("-- ADC data read (div=4) --\n");
  print_samples(samples, REG_ARRAY_ADC_BUFF_SIZE);

  /* 5. Otra lectura con distinto divisor: adc_read debe disparar de nuevo */
  set_adc_clk_divider(8);
  n = adc_read(samples, REG_ARRAY_ADC_BUFF_SIZE);
  if (n != REG_ARRAY_ADC_BUFF_SIZE) {
    return fail("la segunda lectura no devolvio 256 muestras");
  }
  if (!samples_vary(samples, REG_ARRAY_ADC_BUFF_SIZE)) {
    return fail("la segunda lectura no varia");
  }
  print_str("-- ADC segunda lectura (div=8) OK --\n");

  /* 6. Divisor 0: el driver debe forzar 1 y no colgarse */
  set_adc_clk_divider(0);
  n = adc_read(samples, REG_ARRAY_ADC_BUFF_SIZE);
  if (n != REG_ARRAY_ADC_BUFF_SIZE) {
    return fail("la lectura con divisor 0 no devolvio 256 muestras");
  }
  if (REG_ADC_CLK_DIVIDER != 1) {
    return fail("el divisor 0 no se corrigio a 1");
  }
  if (!samples_vary(samples, REG_ARRAY_ADC_BUFF_SIZE)) {
    return fail("la lectura con divisor 0 no varia");
  }
  print_str("-- ADC divisor 0 -> 1 OK --\n");

  REG_FW_REPORT = 0xADC0;
  REG_FW_STATUS = 0xFF;
  print_str("[TEST ADC]: Todos los checks pasaron con exito!\n");

  return 0;
}
