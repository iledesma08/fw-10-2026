#include "dma.h"
#include "config.h"
#include <stddef.h>

void dma_init(void) { REG_DMA_START = 0; }

void dma_copy(const void *src, void *dst, uint32_t n_words) {
  if (n_words == 0 || src == NULL || dst == NULL || src == dst ||
      (((uintptr_t)src & 0x3) != 0 || ((uintptr_t)dst & 0x3) != 0)) {
    return;
  }
  REG_DMA_SOURCE_ADDR = (uint32_t)(uintptr_t)src;
  REG_DMA_DEST_ADDR = (uint32_t)(uintptr_t)dst;
  REG_DMA_SIZE = n_words;
  REG_DMA_START = 0;
  REG_DMA_START = 1;
  while (!dma_is_done()) {
    // Espera a que la copia se complete
  }
  REG_DMA_START = 0;
}

uint8_t dma_is_done(void) { return REG_DMA_DONE; }