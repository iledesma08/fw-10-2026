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

uint8_t adc_done(void) { return (REG_ADC_STATUS & 2u) != 0; }

int adc_read(uint32_t *data, uint32_t size) {
  if (data == NULL || size == 0) {
    return 0;
  }

  if (size > REG_ARRAY_ADC_BUFF_SIZE) {
    size = REG_ARRAY_ADC_BUFF_SIZE;
  }

  if (!adc_done()) {
    /*
     * En w3 de test_drivers.c se llama a adc_read sin haber llamado a adc_start
     * y espera que funcione, asi que supongo que adc_read se encarga de
     * disparar la adquisicion si no se hizo todavia.
     */
    if (REG_ADC_START == 0) {
      adc_start(1);
    }
    while (!adc_done()) {
      // Espera a que termine la adquisición
    }
  }

  for (uint32_t i = 0; i < size; i++) {
    data[i] = REG_ARRAY_ADC_BUFF[i];
  }

  return size;
}

void set_adc_clk_divider(uint32_t clk_div) { REG_ADC_CLK_DIVIDER = clk_div; }