
#ifndef __INTERNAL_H
#define __INTERNAL_H

#include "defines.h"

#include "pagemap.h"
#include "lfbstree.h"

// global variables
// block tree
extern LFBSTree sTree;

// PageMap::UpdatePageInfo wrappers
void SetBlock(TKey key);
void ClearBlock(TKey key);

static inline PageInfo GetPageInfoForPtr(char* ptr)
{
    return sPageMap.GetPageInfo(ptr);
}

// allocate block
// if os = 0, using only internal storage
// if os > 0, if a block cannot be found in internal storage, allocates
//  a block with size max(os, size) from the OS
char* AllocBlock(size_t size, size_t os = HUGEPAGE);
// free a previously allocated block
void FreeBlock(TKey key);
// allocate from OS and add to storage
void ReserveBlockFromOS(size_t size);

#endif // __INTERNAL_H
