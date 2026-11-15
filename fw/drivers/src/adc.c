#include "adc.h"
#include "config.h"

void adc_init(uint32_t clk_div) {
  set_adc_clk_divider(clk_div);
  REG_ADC_START = 0;
}

void adc_start(uint8_t adc_mode) {
  REG_ADC_START = 0;
  REG_ADC_START = adc_mode & 0x3u; // Permite bit 0 (single) y bit 1 (circular)
}

uint8_t adc_half_done(void) { return (REG_ADC_STATUS & 1u) != 0; }

uint8_t adc_done(void) {
  /*
   * Bloquea hasta que la conversion termine. En w3 de test_drivers.c se llama
   * a adc_done() sin polling y luego se verifica el bit de estado (T30).
   */
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
   * Si nunca se configuro el divisor, el hardware calcula i_clk_divider - 1
   * y el contador arranca en 0xFFFFFFFF, colgando la adquisicion.
   */
  if (REG_ADC_CLK_DIVIDER == 0) {
    REG_ADC_CLK_DIVIDER = 1;
  }

  /*
   * adc_read siempre dispara una captura nueva (flanco 0 -> 1) y espera a que
   * termine, sin importar el estado previo de REG_ADC_START ni del buffer.
   */
  adc_start(1);
  adc_done();

  for (uint32_t i = 0; i < size; i++) {
    data[i] = REG_ARRAY_ADC_BUFF[i];
  }

  return size;
}

void set_adc_clk_divider(uint32_t clk_div) { REG_ADC_CLK_DIVIDER = clk_div; }