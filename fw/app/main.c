#include "adc.h"
#include "dma.h"
#include "includes.h"
#include "led.h"
#include "uart.h"

#define N_WORDS 4
#define CLK_DIV 4

/* Verifica la mascara de LEDs contra el valor esperado e informa por UART. */
static int check_led_mask(unsigned int expected) {
  unsigned int mask = led_get_mask();

  uart_write_str("LED mask = 0x");
  print_hex(mask, 2);
  uart_write_str("\n");

  if (mask != expected) {
    uart_write_str(" (se esperaba 0x");
    print_hex(expected, 2);
    uart_write_str(")\n");
    return 0;
  }

  uart_write_str(" OK\n");
  return 1;
}

int main(void) {
  uart_init();
  uart_write_str("[Etapa 1] Inicializando drivers...\n");
  led_init();
  led_on(0); // LED de inicio de la etapa 1
  dma_init();
  adc_init(CLK_DIV);

  uart_write_str("Drivers inicializados. Etapa 1 completada.\n");
  led_on(1); // LED de fin de la etapa 1

  if (!check_led_mask(0x03)) {
    REG_FW_STATUS = 0xBAD;
    return 1;
  }

  uart_write_str("[Etapa 2] Copia de prueba y validacion de DMA...\n");
  led_on(2); // LED de inicio de la etapa 2

  uint32_t src[N_WORDS] = {0xAAAAAAAAu, 0xBBBBBBBBu, 0xCCCCCCCCu, 0xDDDDDDDDu};
  uint32_t dst[N_WORDS] = {0};

  dma_copy(src, dst, N_WORDS);

  for (int i = 0; i < N_WORDS; i++) {
    if (dst[i] != src[i]) {
      uart_write_str("Error: La copia de DMA fallo en la palabra ");
      print_dec(i);
      uart_write_str(".\n");
      REG_FW_STATUS = 0xBAD;
      return 1;
    }
  }

  uart_write_str("Copia de prueba completada. Etapa 2 completada.\n");
  led_on(3); // LED de fin de la etapa 2

  if (!check_led_mask(0x0F)) {
    REG_FW_STATUS = 0xBAD;
    return 1;
  }

  uart_write_str("[Etapa 3] Prueba de ADC...\n");
  led_on(4); // LED de inicio de la etapa 3

  static uint32_t samples[REG_ARRAY_ADC_BUFF_SIZE];

  // Leer 256 muestras del ADC
  int n = adc_read(samples, REG_ARRAY_ADC_BUFF_SIZE);
  if (n != REG_ARRAY_ADC_BUFF_SIZE) {
    uart_write_str("Error: adc_read no devolvio 256 muestras.\n");
    REG_FW_STATUS = 0xBAD;
    return 1;
  }

  // Mostrar las primeras 8 muestras del ADC
  uart_write_str("ADC OK. Primeras 8 muestras:\n");
  for (uint32_t i = 0; i < 8; i++) {
    print_dec(samples[i]);
    print_chr(i == 7 ? '\n' : ' ');
  }

  led_on(5); // LED de fin de la etapa 3

  if (!check_led_mask(0x3F)) {
    REG_FW_STATUS = 0xBAD;
    return 1;
  }

  led_write(0xFF); // 8 LEDs encendidos

  if (!check_led_mask(0xFF)) {
    REG_FW_STATUS = 0xBAD;
    return 1;
  }

  uart_write_str("Demo completa.\n");

  REG_FW_STATUS = 0xFF;

  return 0;
}