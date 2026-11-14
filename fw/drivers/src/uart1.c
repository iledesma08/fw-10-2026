/**
 * @file uart1.c
 * @brief Implementacion del driver para UART1 (UART serie 8N1 con FIFOs de
 * hardware).
 *
 * TP6: completar uart1_set_rts().
 */
#include "uart1.h"

void uart1_init(uint32_t baud_div) {
  REG_UART1_BAUD_DIV = baud_div;
  uart1_clear_errors();
}

uint32_t uart1_rx_level(void) { return UART1_ST_RX_LEVEL(REG_UART1_STATUS); }

int uart1_getc(uint8_t *b) {
  uint32_t rx = REG_UART1_RX_DATA;

  if (rx & UART1_RX_VALID) {
    if (b) {
      *b = (uint8_t)(rx & 0xFF); // Guardar el byte recibido en *b
    }
    return 1; // Había dato
  }

  return 0; // FIFO vacío, no tocamos *b
}

void uart1_putc(uint8_t b) {
  while (REG_UART1_STATUS & UART1_ST_TX_FULL)
    ;
  REG_UART1_TX_DATA = b;
}

void uart1_write(const uint8_t *buf, uint32_t n) {
  if (!buf) {
    return;
  }
  for (uint32_t i = 0; i < n; i++) {
    uart1_putc(buf[i]);
  }
}

int uart1_tx_idle(void) {
  return (REG_UART1_STATUS & UART1_ST_TX_IDLE) ? 1 : 0;
}

uint32_t uart1_errors(void) {
  uint32_t st = REG_UART1_STATUS;
  uint32_t err = 0;

  /* Como los numeros de bit en hardware (2 y 3) no coinciden con los de la
   * funcion (0 y 1) los "traducimos" a los flags de error que devuelve la
   * funcion.
   */
  if (st & UART1_ST_FRAME_ERR) {
    err |= UART1_ERR_FRAME;
  }
  if (st & UART1_ST_OVERRUN) {
    err |= UART1_ERR_OVERRUN;
  }

  return err;
}

void uart1_clear_errors(void) {
  uint32_t rts =
      REG_UART1_CTRL & UART1_CTRL_RTS;       // Guardar el estado del bit RTS
  REG_UART1_CTRL = rts | UART1_CTRL_CLR_ERR; // Limpiar los flags de error
  REG_UART1_CTRL = rts;                      // Restaurar el estado del bit RTS
}

/* ---- TP6 ---- */

void uart1_set_rts(int ready) {
  /* TODO TP6 */
  (void)ready;
}