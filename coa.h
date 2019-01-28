
#ifndef __COA_H
#define __COA_H

#include "defines.h"

// initialize coalescing mechanism
// constructs internal structures, must be called before any alloc/free
void coa_init();

// allocate a block with the requested size, in bytes
void* coa_alloc(size_t size);
// allocate a block with the requested size, in pages
void* coa_alloc_pages(size_t pages);

// deallocate a previously allocated block
void coa_free(void* ptr);

#endif // __COA_H
