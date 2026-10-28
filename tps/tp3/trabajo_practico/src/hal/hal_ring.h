/**
 * @file    hal_ring.h
 * @brief   Buffer circular estatico
 *
 * No usa memoria dinamica: el almacenamiento vive dentro de ring_buf_t
 */

#ifndef HAL_RING_H
#define HAL_RING_H

#include <stdint.h>

/** @brief Capacidad del ring buffer */
#define RING_BUF_SIZE 32

/** @brief Buffer circular de bytes */
typedef struct {
  uint8_t data[RING_BUF_SIZE]; /**< Almacenamiento */
  uint16_t head;               /**< Indice de escritura */
  uint16_t tail;               /**< Indice de lectura */
  uint16_t count;              /**< Ocupacion actual */
} ring_buf_t;

/**
 * @brief Inicializa o reinicia un ring buffer vacio
 * @param rb Buffer a inicializar
 */
void ring_init(ring_buf_t *rb);

/**
 * @brief Encola un byte
 * @param rb   Buffer destino
 * @param byte Byte a encolar
 * @return 0 si se pudo encolar, -1 si el buffer esta lleno
 */
int ring_push(ring_buf_t *rb, uint8_t byte);

/**
 * @brief Desencola un byte
 * @param rb   Buffer origen
 * @param byte Puntero donde se guarda el byte desencolado
 * @return 0 si se pudo desencolar, -1 si el buffer esta vacio
 */
int ring_pop(ring_buf_t *rb, uint8_t *byte);

/**
 * @brief Indica si el ring buffer esta vacio
 * @param rb Buffer a consultar
 * @return Distinto de cero si esta vacio
 */
static inline int ring_is_empty(const ring_buf_t *rb) { return rb->count == 0; }

/**
 * @brief Indica si el ring buffer esta lleno
 * @param rb Buffer a consultar
 * @return Distinto de cero si esta lleno
 */
static inline int ring_is_full(const ring_buf_t *rb) {
  return rb->count >= RING_BUF_SIZE;
}

#endif /* HAL_RING_H */
