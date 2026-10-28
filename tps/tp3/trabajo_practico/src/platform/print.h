/**
 * @file    print.h
 * @brief   Salida de consola
 *
 * print.c implementa el formato sobre print_chr(). Cada plataforma
 * (src/platform/<plataforma>/) implementa print_chr() y
 * print_flush_buffer()
 */

#ifndef PRINT_H
#define PRINT_H

/**
 * @brief Escribe un caracter
 * @param ch Caracter a escribir
 */
void print_chr(char ch);

/**
 * @brief Escribe un string terminado en NUL
 * @param p String a escribir
 */
void print_str(const char *p);

/**
 * @brief Escribe un entero sin signo en decimal
 * @param val Valor a escribir
 */
void print_dec(unsigned int val);

/**
 * @brief Escribe un valor en hexadecimal
 * @param val    Valor a escribir
 * @param digits Cantidad de digitos (se limita a 8)
 */
void print_hex(unsigned int val, int digits);

/**
 * @brief Vacia el buffer de salida de la plataforma
 */
void print_flush_buffer(void);

#endif /* PRINT_H */
