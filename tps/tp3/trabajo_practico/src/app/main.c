/**
 * @file    main.c
 * @brief   Punto de entrada del TP3.
 *
 * Solo orquesta las tres demos.
 */

#include "demo.h"

int main(void)
{
    demo_math();
    demo_str();
    demo_embedded();

    return 0;
}
