// Allocator override compiled together with the chipmunk sources. The source
// files call cpcalloc()/cpfree()/cprealloc(), which this translation unit
// defines after the macros have been redirected at compile time.

#include <stdlib.h>
#include <string.h>

typedef struct Header { size_t size; } Header;

size_t cp_alloc_current_bytes(void);
size_t cp_alloc_peak_bytes(void);
void cp_alloc_reset(void);

static size_t currentBytes = 0;
static size_t peakBytes = 0;

static void
track(size_t size)
{
	currentBytes += size;
	if(currentBytes > peakBytes) peakBytes = currentBytes;
}

void *
cp_calloc(size_t nmemb, size_t size)
{
	size_t total = nmemb * size;
	void *raw = malloc(total + sizeof(Header));
	if(!raw) return NULL;
	((Header *)raw)->size = total;
	memset((char *)raw + sizeof(Header), 0, total);
	track(total);
	return (char *)raw + sizeof(Header);
}

void
cp_free(void *ptr)
{
	if(!ptr) return;
	Header *header = (Header *)ptr - 1;
	currentBytes -= header->size;
	free(header);
}

void *
cp_realloc(void *ptr, size_t size)
{
	if(!ptr) return cp_calloc(1, size);
	Header *old = (Header *)ptr - 1;
	currentBytes -= old->size;

	void *raw = realloc(old, size + sizeof(Header));
	if(!raw) return NULL;
	((Header *)raw)->size = size;
	currentBytes += size;
	if(currentBytes > peakBytes) peakBytes = currentBytes;
	return (char *)raw + sizeof(Header);
}

size_t
cp_alloc_current_bytes(void)
{
	return currentBytes;
}

size_t
cp_alloc_peak_bytes(void)
{
	return peakBytes;
}

void
cp_alloc_reset(void)
{
	currentBytes = 0;
	peakBytes = 0;
}
