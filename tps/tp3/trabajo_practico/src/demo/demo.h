/**
 * @file    demo.h
 * @brief   Funciones de demostracion
 *
 * Cada funcion ejecuta una parte de la salida del programa y no recibe
 * parametros
 */

#ifndef DEMO_H
#define DEMO_H

/**
 * @brief Ejecuta las pruebas de las funciones matemáticas.
 */
void demo_math(void);

/**
 * @brief Ejecuta las pruebas de strings y CRC-8.
 */
void demo_str(void);

/**
 * @brief Ejecuta la prueba de integración del frame, el ring y el timer.
 */
void demo_embedded(void);

#endif /* DEMO_H */
