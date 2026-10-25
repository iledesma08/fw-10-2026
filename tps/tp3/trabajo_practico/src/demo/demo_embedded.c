/**
 * @file    demo_embedded.c
 * @brief   Demo de los periféricos simulados e integración (hitos 6 a 9).
 *
 * UART + ring buffer + trama con CRC-8 + timer. Toda la salida se hace con
 * print.h (print_str / print_dec / print_hex / print_chr).
 */

#include <stddef.h>

#include "demo.h"
#include "strutils.h"
#include "hal_uart.h"
#include "hal_ring.h"
#include "hal_timer.h"
#include "print.h"

#define FRAME_SIZE  6U

static uart_regs_t sim_uart;
static ring_buf_t  sim_ring;

void demo_embedded(void)
{
    static const uint8_t msg[] = { 'O', 'K' };
    static const uint8_t payload[] = { 0x01U, 0x02U, 0x03U, 0x04U };
    uint8_t frame[FRAME_SIZE];
    uint8_t received[FRAME_SIZE] = { 0U };
    uint8_t byte;
    uint8_t crc_expected;
    uint8_t crc_received = 0U;
    int frame_ok;

    /* Hito 6: UART */
    uart_init(&sim_uart, 115200U);
    uart_send(&sim_uart, msg, (uint32_t)sizeof(msg));

    print_str("uart BRR=");
    print_dec((unsigned int)sim_uart.BRR);
    print_str(" SR=0x");
    print_hex((unsigned int)sim_uart.SR, 8);
    print_str(" DR=0x");
    print_hex((unsigned int)sim_uart.DR, 8);
    print_str("\n");

    /* Hito 7: ring buffer */
    ring_init(&sim_ring);
    for (uint8_t i = 0U; i < 10U; i++)
        (void)ring_push(&sim_ring, (uint8_t)('A' + i));

    print_str("ring: ");
    while (ring_pop(&sim_ring, &byte) == 0)
        print_chr((char)byte);
    print_str("\n");

    /* Hito 9: trama por el ring, con validación de CRC ─────── */
    frame[0] = 0xAAU;
    for (size_t i = 0; i < sizeof(payload); i++)
        frame[1U + i] = payload[i];

    crc_expected = str_crc8(payload, sizeof(payload));
    frame[FRAME_SIZE - 1U] = crc_expected;

    ring_init(&sim_ring);
    frame_ok = 1;
    for (size_t i = 0; i < FRAME_SIZE; i++) {
        if (ring_push(&sim_ring, frame[i]) != 0)
            frame_ok = 0;
    }
    for (size_t i = 0; i < FRAME_SIZE; i++) {
        if (ring_pop(&sim_ring, &received[i]) != 0)
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

    /* Hito 8/9: timer */
    timer_init();
    timer_tick();
    timer_tick();
    timer_tick();

    print_str("timer_ms: ");
    print_dec((unsigned int)timer_get_ms());
    print_str("\n");
}
