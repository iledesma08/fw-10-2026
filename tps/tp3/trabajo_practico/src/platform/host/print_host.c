#include "print.h"
#include <stdio.h>

void print_chr(char ch) { putchar(ch); }

void print_flush_buffer(void) { fflush(stdout); }
