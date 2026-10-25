/**
 * @file    mathlib.c
 * @brief   Biblioteca matemática entera del TP3.
 */

#include "mathlib.h"

int32_t math_factorial(int32_t n)
{
    int32_t result = 1;

    if (n < 0)
        return 0;

    for (int32_t i = 2; i <= n; i++)
        result *= i;

    return result;
}

int32_t math_gcd(int32_t a, int32_t b)
{
    if (a < 0)
        a = -a;
    if (b < 0)
        b = -b;

    while (b != 0) {
        int32_t tmp = b;
        b = a % b;
        a = tmp;
    }

    return a;
}

uint32_t math_isqrt(uint32_t n)
{
    uint32_t x;
    uint32_t y;

    if (n == 0)
        return 0;

    /* Método de Newton para la raíz cuadrada entera. */
    x = n;
    y = (x + 1) / 2;
    while (y < x) {
        x = y;
        y = (x + n / x) / 2;
    }

    return x;
}

int math_is_prime(int32_t n)
{
    if (n < 2)
        return 0;
    if (n < 4)
        return 1;
    if (n % 2 == 0 || n % 3 == 0)
        return 0;

    for (int32_t i = 5; (int64_t)i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0)
            return 0;
    }

    return 1;
}
