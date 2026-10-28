/**
 * @file    print_host.c
 * @brief   Backend de impresion para la plataforma host
 */

#include "print.h"
#include <stdio.h>

void print_chr(char ch) { putchar(ch); }

void print_flush_buffer(void) { fflush(stdout); }
