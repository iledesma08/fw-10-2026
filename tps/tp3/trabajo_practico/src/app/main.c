/**
 * @file    main.c
 * @brief   Punto de entrada del TP3.
 */

#include "mathlib.h"
#include "strutils.h"
#include "print.h"

int main(void)
{
    strbuf_t sb;
    char trimmed[STR_MAX_LEN];
    static const uint8_t payload[] = { 0x01U, 0x02U, 0x03U, 0x04U };
    uint8_t crc;

    print_str("factorial(5) = ");
    print_dec((unsigned int)math_factorial(5));
    print_str("\n");

    print_str("gcd(48, 18) = ");
    print_dec((unsigned int)math_gcd(48, 18));
    print_str("\n");

    print_str("isqrt(17) = ");
    print_dec((unsigned int)math_isqrt(17));
    print_str("\n");

    print_str("is_prime(17) = ");
    print_dec((unsigned int)math_is_prime(17));
    print_str("\n");

    print_str("is_prime(18) = ");
    print_dec((unsigned int)math_is_prime(18));
    print_str("\n");

    strbuf_init(&sb);
    strbuf_set(&sb, "value=");
    strbuf_append_int(&sb, 42);
    print_str(strbuf_cstr(&sb));
    print_str("\n");

    str_trim(trimmed, "  hello  ", sizeof(trimmed));
    print_str("trimmed: '");
    print_str(trimmed);
    print_str("'\n");

    crc = str_crc8(payload, sizeof(payload));
    print_str("crc8: 0x");
    print_hex((unsigned int)crc, 2);
    print_str("\n");

    return 0;
}
