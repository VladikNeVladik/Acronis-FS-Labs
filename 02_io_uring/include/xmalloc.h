// No copyright. 2021, Vladislav Aleinik
#ifndef IO_RING_COPY_XMALLOC_H
#define IO_RING_COPY_XMALLOC_H

#include <stdint.h>

void* xmalloc(size_t size);

void* aligned_xalloc(size_t alignment, size_t size);

#endif // IO_RING_COPY_XMALLOC_H
