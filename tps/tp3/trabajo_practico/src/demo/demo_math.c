/**
 * @file    demo_math.c
 * @brief   Demo del subsistema mathlib (hitos 2 y 3)
 */

#include "demo.h"
#include "mathlib.h"
#include "print.h"

void demo_math(void) {
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
}
