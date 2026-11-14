#include "uart.h"
#include "config.h"

void uart_init(void) {
  REG_UART0_START_TRANSMIT = 0;
  REG_UART0_DATA_FLUSH = 1;
  while (!REG_UART0_TRANSMISION_DONE)
    ;
}

/**
 * @brief Funcion auxiliar para enviar un buffer de datos por UART.
 * @param buf Buffer a enviar.
 * @param len El tamaño del buffer a enviar.
 */
static void uart_send_buffer(const char *buf, uint32_t len) {
  if (!buf || len == 0) {
    return;
  }
  // Limito la longitud a enviar al tamano maximo del buffer de salida
  if (len > REG_ARRAY_UART0_BUFF_OUT_SIZE) {
    len = REG_ARRAY_UART0_BUFF_OUT_SIZE;
  }
  while (!REG_UART0_TRANSMISION_DONE)
    ;
  for (uint32_t i = 0; i < len; i++) {
    REG_ARRAY_UART0_BUFF_OUT[i] = (unsigned char)buf[i];
  }
  REG_UART0_SIZE_TO_TRANSMIT = len;
  REG_UART0_START_TRANSMIT = 1;
  REG_UART0_START_TRANSMIT = 0;
  while (!REG_UART0_TRANSMISION_DONE)
    ;
}

void uart_write_byte(char c) { uart_send_buffer(&c, 1); }

void uart_write_str(const char *s) {
  if (!s || s[0] == '\0') {
    return;
  }
  uint32_t len = 0;
  while (s[len] != '\0' && len < REG_ARRAY_UART0_BUFF_OUT_SIZE) {
    len++;
  }
  uart_send_buffer(s, len);
}
