
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

char* AllocBlock(size_t size);
void FreeBlock(TKey key);

#endif // __INTERNAL_H
