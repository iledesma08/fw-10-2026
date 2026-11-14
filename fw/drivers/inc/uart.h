/**
 * @file uart.h
 * @brief Driver para el periférico UART0 (transmisión por buffer).
 */

#ifndef UART_H
#define UART_H

#include <stdint.h>

/**
 * @brief Inicializa el periférico UART.
 *
 * Esta función configura el periférico UART para su uso.
 */
void uart_init(void);

/**
 * @brief Envía un solo byte a través del periférico UART.
 *
 * @param c El byte a enviar.
 */
void uart_write_byte(char c);

/**
 * @brief Envía una cadena de caracteres a través de UART.
 *
 * @param s La cadena de caracteres a enviar. Debe ser una cadena terminada en
 * nulo.
 */
void uart_write_str(const char *s);

#endif /* UART_H */