/**
 * @file    hal_timer.h
 * @brief   Temporizador (1 tick = 1 ms)
 */

#ifndef HAL_TIMER_H
#define HAL_TIMER_H

#include <stdint.h>

/**
 * @brief Inicializa el temporizador en cero
 */
void timer_init(void);

/**
 * @brief Avanza el temporizador 1 ms
 */
void timer_tick(void);

/**
 * @brief Devuelve el tiempo transcurrido
 * @return Milisegundos acumulados desde timer_init()
 */
uint32_t timer_get_ms(void);

#endif /* HAL_TIMER_H */
