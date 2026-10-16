#ifndef MEDIA_MEMORY_H
#define MEDIA_MEMORY_H
#include <stdint.h>
#define MEDIA_PERSIST_BYTES 145024U
#define MEDIA_SCRATCH_BYTES 182656U
/* Capture and encoding are sequential. The encoder may overwrite a consumed frame.
 * File operations use the persistent area only while recording is stopped. */
extern uint8_t Media_Memory[MEDIA_PERSIST_BYTES + MEDIA_SCRATCH_BYTES];
#define MEDIA_CAPTURE_BUFFER (Media_Memory + MEDIA_PERSIST_BYTES)
#endif
