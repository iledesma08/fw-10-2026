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

    return 0;
}
