// Forced-include declarations for the tracking allocator.
#ifndef CP_ALLOC_OVERRIDE_H
#define CP_ALLOC_OVERRIDE_H

#include <stddef.h>

void *cp_calloc(size_t nmemb, size_t size);
void cp_free(void *ptr);
void *cp_realloc(void *ptr, size_t size);
void cp_alloc_reset(void);

#endif
