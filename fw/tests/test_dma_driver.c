#include "dma.h"
#include "includes.h"
#include <stddef.h>

#define TEST_SIZE 32

/* Creamos los arreglos con 4 palabras extra de margen para verificar que el
 * hardware no escriba fuera de los limites */
static uint32_t src_buf[TEST_SIZE + 4];
static uint32_t dst_buf[TEST_SIZE + 4];

int main(void) {
  REG_FW_STATUS = 0x0;
  REG_FW_REPORT = 0x0;

  print_str("[TEST DMA]: Iniciando test del driver de DMA...");

  /* 1. Inicializacion */
  dma_init();

  /* 2. Copia normal de memoria a memoria */
  for (int i = 0; i < TEST_SIZE + 4; i++) {
    src_buf[i] = 0x12340000 + i;
    dst_buf[i] = 0x23450000;
  }

  dma_copy(src_buf, dst_buf,
           TEST_SIZE); // DMA deberia copiar solamente TEST_SIZE palabras, no
                       // las 4 palabras extra de margen

  /* Verificar que la copia se haya completado */
  if (!dma_is_done()) {
    REG_FW_STATUS = 0xBAD;
    return -1;
  }

  /* Verificar que las primeras TEST_SIZE palabras coincidan */
  for (int i = 0; i < TEST_SIZE; i++) {
    if (dst_buf[i] != src_buf[i]) {
      REG_FW_STATUS = 0xBAD;
      return -1;
    }
  }

  /* Verificar que los elementos fuera del rango no se hayan modificado */
  for (int i = TEST_SIZE; i < TEST_SIZE + 4; i++) {
    if (dst_buf[i] != 0x23450000) {
      REG_FW_STATUS = 0xBAD;
      return -1;
    }
  }

  /* 3. Comprobacion de casos invalidos (guardas defensivas) */
  /* 3.a n_words == 0 */
  dst_buf[0] = 0x34560000;
  dma_copy(src_buf, dst_buf, 0);
  if (dst_buf[0] != 0x34560000) {
    REG_FW_STATUS = 0xBAD;
    return -1;
  }

  /* 3.b Puntero nulo (src == NULL) */
  dst_buf[0] = 0x55555555;
  dma_copy(NULL, dst_buf, 4);
  if (dst_buf[0] != 0x55555555) {
    REG_FW_STATUS = 0xBAD;
    return -1;
  }

  /* 3.c Puntero nulo (dst == NULL) */
  dst_buf[0] = 0x66666666;
  dma_copy(src_buf, NULL, 4);
  if (dst_buf[0] != 0x66666666) {
    REG_FW_STATUS = 0xBAD;
    return -1;
  }

  /* 3.d Puntero src no alineado a 4 bytes */
  dst_buf[0] = 0xAAAAAAAA;
  const void *unaligned_src =
      (const void *)((uintptr_t)src_buf +
                     1); // El +1 hace que el puntero no este alineado a 4 bytes
                         // sino a 1 byte
  dma_copy(unaligned_src, dst_buf, 4);
  if (dst_buf[0] != 0xAAAAAAAA) {
    REG_FW_STATUS = 0xBAD;
    return -1;
  }

  /* 3.d Puntero dst no alineado a 4 bytes  */
  void *unaligned_dst =
      (void *)((uintptr_t)dst_buf + 2); // El +2 hace que el puntero no este
                                        // alineado a 4 bytes sino a 2 bytes
  dma_copy(src_buf, unaligned_dst, 4);
  if (dst_buf[0] != 0xAAAAAAAA) {
    REG_FW_STATUS = 0xBAD;
    return -1;
  }

  /* 3.e src == dst */
  dst_buf[0] = 0x11223344;
  dma_copy(dst_buf, dst_buf, 4);
  if (dst_buf[0] != 0x11223344) {
    REG_FW_STATUS = 0xBAD;
    return -1;
  }

  /* 3.f Direccion fuera del mapa (src no mapeado) */
  dst_buf[0] = 0x778899AA;
  dma_copy((const void *)0x00100000u, dst_buf, 16);
  if (dst_buf[0] != 0x778899AA) {
    REG_FW_STATUS = 0xBAD;
    return -1;
  }

  /* 3.g Direccion fuera del mapa (dst no mapeado) */
  src_buf[0] = 0xBBCCDDEE;
  dma_copy(src_buf, (void *)0x00100000u, 16);
  if (src_buf[0] != 0xBBCCDDEE) {
    REG_FW_STATUS = 0xBAD;
    return -1;
  }

  /* 3.h Rango que empieza adentro pero cruza el limite 0x00060000 */
  dst_buf[0] = 0x12345678;
  dma_copy((const void *)0x0005FFF8u, dst_buf, 4);
  if (dst_buf[0] != 0x12345678) {
    REG_FW_STATUS = 0xBAD;
    return -1;
  }

  /* 4. Copias de tamano variable (multiples transferencias sucesivas) */
  for (uint32_t size = 1; size <= 16; size++) {
    for (uint32_t i = 0; i < size; i++) {
      src_buf[i] = 0xA000 + (size << 4) + i;
      dst_buf[i] = 0;
    }
    dma_copy(src_buf, dst_buf, size);
    if (!dma_is_done()) {
      REG_FW_STATUS = 0xBAD;
      return -1;
    }
    for (uint32_t i = 0; i < size; i++) {
      if (dst_buf[i] != src_buf[i]) {
        REG_FW_STATUS = 0xBAD;
        return -1;
      }
    }
  }

  print_str("[TEST DMA]: Todos los checks pasaron con exito!");
  REG_FW_STATUS = 0xFF;
  return 0;
}
