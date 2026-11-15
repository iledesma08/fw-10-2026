#ifndef DMA_H
#define DMA_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Inicializa el registro de DMA. Limpia el registro poniendo el estado
 * de inicio en 0 para garantizar que el DMA quede listo para detectar el
 * proximo flanco ascendente y asi comenzar la copia
 */
void dma_init(void);

/**
 * @brief Copia n_words desde la direccion src a la direccion dst utilizando el
 * DMA. Espera hasta que la operacion se complete
 * @param src Direccion de origen de la copia
 * @param dst Direccion de destino de la copia
 * @param n_words Numero de palabras (4 bytes cada una) a copiar
 *
 * Si n_words es 0, src o dst son NULL, src y dst son iguales, o si las
 * direcciones no estan alineadas a 4 bytes, la funcion no realiza ninguna
 * accion
 *
 * @warning Los buffers src y dst no deben solaparse con dst > src. El hardware
 * realiza la copia en orden ascendente de direcciones, por lo que un
 * solapamiento donde dst > src sobreescribe datos de origen antes de leerlos
 * (comportamiento analogo a memcpy)
 */
void dma_copy(const void *src, void *dst, uint32_t n_words);

/**
 * @brief Verifica si la operacion de copia del DMA ha finalizado retornando el
 * estado del registro `REG_DMA_DONE`
 * @return 1 si la operacion ha finalizado, 0 en caso contrario
 */
uint8_t dma_is_done(void);

#endif /* DMA_H */