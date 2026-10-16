#include "media_memory.h"
uint8_t Media_Memory[MEDIA_PERSIST_BYTES + MEDIA_SCRATCH_BYTES]
  __attribute__((section(".bss.dma_buffer"), aligned(32)));
