// No copyright. 2021, Vladislav Aleinik

#define _ISOC11_SOURCE

#include <stdlib.h>
#include <memory.h>
#include <logging.h>

void* xmalloc(size_t size)
{
	void* ret = malloc(size);
	BUG_ON(ret == NULL, "Memory allocation failed");

	return ret;
}

void* aligned_xalloc(size_t alignment, size_t size)
{
	void* ret = aligned_alloc(alignment, size);
	BUG_ON(ret == NULL, "Aligned memory allocation failed");

	return ret;
}