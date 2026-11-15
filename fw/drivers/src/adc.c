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
   * test_drivers.c nunca llama a adc_init(): w3 usa adc_read() de una y recien
   * al final setea REG_ADC_CLK_DIVIDER, por lo que al llegar aca el registro
   * tiene el valor 0.
   *
   * En rtl/adc.sv el contador se recarga con i_clk_divider - 1; con un divisor
   * 0 arranca en 0xFFFFFFFF y la primera muestra tarda aprox 2^32 ciclos. Esto
   * hace que la simulacion no imprima ningun Txx y que termine por MAX_CYCLES.
   * Forzar 1 permite usar el divisor minimo valido.
   */
  if (REG_ADC_CLK_DIVIDER == 0) {
    REG_ADC_CLK_DIVIDER = 1;
  }

  /*
   * Se dispara siempre una captura nueva en vez de depender de REG_ADC_START
   * o del bit done previos.
   */
  adc_start(1);
  adc_done();

  for (uint32_t i = 0; i < size; i++) {
    data[i] = REG_ARRAY_ADC_BUFF[i];
  }

  return size;
}

void set_adc_clk_divider(uint32_t clk_div) { REG_ADC_CLK_DIVIDER = clk_div; }