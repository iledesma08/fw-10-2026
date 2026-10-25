/**
 * @file    hal_ring.c
 * @brief   Buffer circular estático del TP3.
 *
 * La capacidad la define RING_BUF_SIZE (32 en este TP) en hal_ring.h.
 */

#include "hal_ring.h"

void ring_init(ring_buf_t *rb)
{
    for (uint16_t i = 0; i < RING_BUF_SIZE; i++)
        rb->data[i] = 0U;

    rb->head = 0U;
    rb->tail = 0U;
    rb->count = 0U;
}

int ring_push(ring_buf_t *rb, uint8_t byte)
{
    if (ring_is_full(rb))
        return -1;

    rb->data[rb->head] = byte;
    rb->head = (uint16_t)((rb->head + 1U) % RING_BUF_SIZE);
    rb->count++;

    return 0;
}

int ring_pop(ring_buf_t *rb, uint8_t *byte)
{
    if (ring_is_empty(rb))
        return -1;

    *byte = rb->data[rb->tail];
    rb->tail = (uint16_t)((rb->tail + 1U) % RING_BUF_SIZE);
    rb->count--;

    return 0;
}
