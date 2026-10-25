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
