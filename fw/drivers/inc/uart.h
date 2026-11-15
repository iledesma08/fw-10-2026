/**
 * @file uart.h
 * @brief Driver para el periferico UART0 (transmision por buffer)
 */

#ifndef UART_H
#define UART_H

#include <stdint.h>

/**
 * @brief Inicializa el periferico UART
 */
void uart_init(void);

/**
 * @brief Envia un solo byte a traves del periferico UART
 *
 * @param c El byte a enviar
 */
void uart_write_byte(char c);

/**
 * @brief Envia una cadena de caracteres a traves de UART
 *
 * @param s La cadena de caracteres a enviar. Debe ser una cadena terminada en
 * nulo
 */
void uart_write_str(const char *s);

#endif /* UART_H */