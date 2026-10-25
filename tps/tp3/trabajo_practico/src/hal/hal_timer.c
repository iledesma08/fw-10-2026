/**
 * @file    hal_timer.c
 * @brief   Temporizador software del TP3 (1 tick = 1 ms).
 *
 * No existe en las actividades de clase: la implementación sale del header.
 */

#include "hal_timer.h"

static uint32_t timer_ms;

void timer_init(void)
{
    timer_ms = 0U;
}

void timer_tick(void)
{
    timer_ms++;
}

uint32_t timer_get_ms(void)
{
    return timer_ms;
}
