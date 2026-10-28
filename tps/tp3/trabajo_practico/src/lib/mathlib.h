/**
 * @file    mathlib.h
 * @brief   Funciones matematicas enteras del TP3
 *
 * Sin memoria dinamica: todas las funciones son reentrantes
 */

#ifndef MATHLIB_H
#define MATHLIB_H

#include <stdint.h>

/**
 * @brief Calcula el factorial de un entero en forma iterativa
 * @param n Entero no negativo
 * @return n!, o 0 si @p n es negativo. Se desborda silenciosamente para
 *         valores grandes
 */
int32_t math_factorial(int32_t n);

/**
 * @brief Calcula el maximo comun divisor con el algoritmo de Euclides
 * @param a Primer entero -> se ignora su signo
 * @param b Segundo entero -> se ignora su signo
 * @return gcd(|a|, |b|)
 */
int32_t math_gcd(int32_t a, int32_t b);

/**
 * @brief Calcula la raiz cuadrada entera por el metodo de Newton
 * @param n Entero no negativo
 * @return Parte entera de la raiz cuadrada de @p n
 */
uint32_t math_isqrt(uint32_t n);

/**
 * @brief Determina si un entero es primo por divisiones 6k + 1 y 6k - 1
 * @param n Entero a analizar
 * @return Distinto de cero si @p n es primo, 0 en caso contrario
 */
int math_is_prime(int32_t n);

/**
 * @brief Prende un bit en un valor de registro
 * @param reg Valor actual del registro
 * @param bit Posicion del bit, entre 0 y 31
 * @return Valor de @p reg con el bit @p bit prendido
 */
static inline uint32_t bit_set(uint32_t reg, uint8_t bit) {
  return reg | (1U << bit);
}

#endif /* MATHLIB_H */
