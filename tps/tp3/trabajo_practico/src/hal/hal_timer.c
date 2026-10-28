/**
 * @file    hal_timer.c
 * @brief   Implementacion del temporizador software del TP3
 */

#include "hal_timer.h"

static uint32_t timer_ms;

void timer_init(void) { timer_ms = 0U; }

void timer_tick(void) { timer_ms++; }

uint32_t timer_get_ms(void) { return timer_ms; }
