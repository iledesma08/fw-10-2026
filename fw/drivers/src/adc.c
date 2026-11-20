#include "adc.h"
#include "config.h"

void adc_init(uint32_t clk_div) {
  REG_ADC_CLK_DIVIDER = clk_div;
  REG_ADC_START = 0;
}

void adc_start(uint8_t adc_mode) {
  REG_ADC_START = 0;
  REG_ADC_START = adc_mode & 0x3u; // Permite bit 0 (single) y bit 1 (circular)
}

uint8_t adc_half_done(void) {
  while ((REG_ADC_STATUS & 1u) == 0) {
  }
  return 1;
}

uint8_t adc_done(void) {
  while ((REG_ADC_STATUS & 2u) == 0) {
  }
  return 1;
}

int adc_read(uint32_t *data, uint32_t size) {
  if (data == NULL || size == 0) {
    return 0;
  }

  if (size > REG_ARRAY_ADC_BUFF_SIZE) {
    size = REG_ARRAY_ADC_BUFF_SIZE;
  }

  /*
   * Usa el divisor de reloj ya configurado y dispara siempre una captura unica
   * nueva en vez de depender de REG_ADC_START o del bit done previos
   */
  adc_start(1);
  adc_done();

  for (uint32_t i = 0; i < size; i++) {
    data[i] = REG_ARRAY_ADC_BUFF[i];
  }

  return size;
}