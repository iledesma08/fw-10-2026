/**
 * @file    demo_embedded.c
 * @brief   Integración de la trama con CRC-8 y el timer (hito 9).
 */

#include <stddef.h>

#include "demo.h"
#include "strutils.h"
#include "hal_ring.h"
#include "hal_timer.h"
#include "print.h"

#define FRAME_SIZE  6U

void demo_embedded(void)
{
    static const uint8_t payload[] = { 0x01U, 0x02U, 0x03U, 0x04U };
    uint8_t frame[FRAME_SIZE];
    uint8_t received[FRAME_SIZE] = { 0U };
    uint8_t crc_expected;
    uint8_t crc_received = 0U;
    ring_buf_t ring;
    int frame_ok;

    frame[0] = 0xAAU;
    for (size_t i = 0; i < sizeof(payload); i++)
        frame[1U + i] = payload[i];

    crc_expected = str_crc8(payload, sizeof(payload));
    frame[FRAME_SIZE - 1U] = crc_expected;

    ring_init(&ring);
    frame_ok = 1;
    for (size_t i = 0; i < FRAME_SIZE; i++) {
        if (ring_push(&ring, frame[i]) != 0)
            frame_ok = 0;
    }
    for (size_t i = 0; i < FRAME_SIZE; i++) {
        if (ring_pop(&ring, &received[i]) != 0)
            frame_ok = 0;
    }

    if (frame_ok && received[0] == 0xAAU) {
        crc_received = str_crc8(&received[1], FRAME_SIZE - 2U);
        if (crc_received != received[FRAME_SIZE - 1U])
            frame_ok = 0;
    } else {
        frame_ok = 0;
    }

    if (frame_ok) {
        print_str("frame: ok crc=0x");
        print_hex((unsigned int)crc_received, 2);
        print_str("\n");
    } else {
        print_str("frame: error\n");
    }

    timer_init();
    timer_tick();
    timer_tick();
    timer_tick();

    print_str("timer_ms: ");
    print_dec((unsigned int)timer_get_ms());
    print_str("\n");
}
