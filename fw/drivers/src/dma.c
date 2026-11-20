#include "dma.h"
#include "config.h"
#include <stddef.h>

void dma_init(void) { REG_DMA_START = 0; }

#define DMA_MEM_START ((uintptr_t)ADDR_START_INST)
#define DMA_MEM_END   ((uintptr_t)(ADDR_START_REG_FILE + ADDR_SIZE_REG_FILE))

static int dma_range_valid(uintptr_t addr, uint32_t n_words) {
  /* Mapa valido: 0x00000000 (ADDR_START_INST) hasta 0x0005FFFF (DMA_MEM_END - 1) */
  if (addr < DMA_MEM_START || addr >= DMA_MEM_END) {
    return 0;
  }
  if (n_words > (uint32_t)((DMA_MEM_END - addr) >> 2)) {
    return 0;
  }
  return 1;
}

void dma_copy(const void *src, void *dst, uint32_t n_words) {
  if (n_words == 0 || src == NULL || dst == NULL || src == dst ||
      (((uintptr_t)src & 0x3) != 0 || ((uintptr_t)dst & 0x3) != 0) ||
      !dma_range_valid((uintptr_t)src, n_words) ||
      !dma_range_valid((uintptr_t)dst, n_words)) {
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