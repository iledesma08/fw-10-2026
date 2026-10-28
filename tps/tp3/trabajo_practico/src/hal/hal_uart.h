/**
 * @file    hal_uart.h
 * @brief   Driver de una UART simulada con registros en memoria
 *
 * No hay hardware real: el estado vive en un uart_regs_t que provee el
 * llamador, asi que pueden coexistir varias instancias
 */

#ifndef HAL_UART_H
#define HAL_UART_H

#include "mathlib.h"
#include <stdint.h>

/** @brief Reloj del periferico UART: 16 MHz */
#define UART_PERIPH_CLK_HZ 16000000U

/** @brief Registros de la UART simulada */
typedef struct {
  volatile uint32_t CR1; /**< Control 1: UE, TE y RE */
  volatile uint32_t CR2; /**< Control 2: reservado en esta simulacion (no se usa
                            pero venia asi del commit inicial) */
  volatile uint32_t SR;  /**< Estado: bit TC, entre otros */
  volatile uint32_t DR;  /**< Dato: ultimo byte transmitido */
  volatile uint32_t BRR; /**< Baud rate: reloj / baudrate */
} uart_regs_t;

/**
 * @brief Inicializa una instancia de UART
 *
 * Calcula BRR = UART_PERIPH_CLK_HZ / baudrate, prende UE, TE y RE en
 * CR1 y limpia SR y DR, y @p baudrate debe ser distinto de cero
 *
 * @param uart     Registros a inicializar
 * @param baudrate Velocidad en baudios
 */
void uart_init(uart_regs_t *uart, uint32_t baudrate);

/**
 * @brief Transmite un byte
 * @param uart Registros de una UART inicializada
 * @param byte Byte a enviar
 */
void uart_send_byte(uart_regs_t *uart, uint8_t byte);

/**
 * @brief Transmite un buffer completo
 * @param uart Registros de una UART inicializada
 * @param data Puntero a los datos
 * @param len  Cantidad de bytes a enviar
 */
void uart_send(uart_regs_t *uart, const uint8_t *data, uint32_t len);

#endif /* HAL_UART_H */
