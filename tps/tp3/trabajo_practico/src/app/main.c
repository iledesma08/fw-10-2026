/**
 * @file    main.c
 * @brief   Punto de entrada del TP3.
 */

#include "mathlib.h"
#include "print.h"

int main(void)
{
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

    return 0;
}
