/**
 * @file    hal_uart.c
 * @brief   Driver de la UART simulada
 *
 * El periferico es una estructura de registros en memoria. No hay hardware
 * real, el reloj del periferico es UART_PERIPH_CLK_HZ (16 MHz en este TP)
 */

#include "hal_uart.h"

void uart_init(uart_regs_t *uart, uint32_t baudrate) {
  uart->BRR = UART_PERIPH_CLK_HZ / baudrate;

  uart->CR1 = bit_set(0U, 13);       /* UE: UART enable */
  uart->CR1 = bit_set(uart->CR1, 3); /* TE: transmitter enable */
  uart->CR1 = bit_set(uart->CR1, 2); /* RE: receiver enable */

  uart->SR = 0U;
  uart->DR = 0U;
}

void uart_send_byte(uart_regs_t *uart, uint8_t byte) {
  uart->DR = byte;
  uart->SR = bit_set(uart->SR, 6); /* TC: transfer complete */
}

void uart_send(uart_regs_t *uart, const uint8_t *data, uint32_t len) {
  for (uint32_t i = 0; i < len; i++)
    uart_send_byte(uart, data[i]);
}
