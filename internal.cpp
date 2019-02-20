
#include <cstring> // for memset/memcpy
#include <algorithm> // for max()

#include "log.h" // for ASSERT
#include "pages.h"

#include "internal.h"

// global variables
// block tree
LFBSTree sTree;

// PageMap::UpdatePageInfo wrappers
void SetBlock(TKey key)
{
    char* ptr = key.address;
    size_t size = key.size;
    // block must be cleared before setting
    // set block start
    bool res = sPageMap.UpdatePageInfo(ptr, PageInfo(0), PageInfo(size));
    (void)res; // suppress unused warning
    ASSERT(res);

    // set block end (only if block isn't a single-page block)
    if (size == PAGE)
        return;

    res = sPageMap.UpdatePageInfo(ptr + size - PAGE, PageInfo(0), PageInfo(-size));
    (void)res; // suppress unused warning
    ASSERT(res);
}

void ClearBlock(TKey key)
{
    char* ptr = key.address;
    size_t size = key.size;
    // clear start of block
    bool res = sPageMap.UpdatePageInfo(ptr, PageInfo(size), PageInfo(0));
    (void)res; // suppress unused warning
    ASSERT(res);

    // clear end of block (if block isn't a single-page block)
    res = sPageMap.UpdatePageInfo(ptr + size - PAGE, PageInfo(-size), PageInfo(0));
}

// allocate block
// if os = 0, using only internal storage
// if os > 0, if a block cannot be found in internal storage, allocates
//  a block with size max(os, size) from the OS
char* AllocBlock(size_t size, size_t os /*= HUGEPAGE*/)
{
    if (UNLIKELY(size == 0))
        size = PAGE;

    ASSERT((size & PAGE_MASK) == 0);

    TKey key(size);
    if (!sTree.RemoveNext(key))
    {
        if (os == 0)
            return nullptr;

        // no available blocks
        // alloc a large block and carve from it
        size_t blockSize = std::max(size, os);
        char* block = (char*)PageAlloc(blockSize);
        if (UNLIKELY(block == nullptr))
            return nullptr;

        key = TKey(blockSize, block);
        // update page map
        SetBlock(key);
    }

    // obtained a block, check size and split if needed
    ASSERT(key.size >= size);

    // not exact match, need to split block
    if (key.size > size)
    {
        // clear page map info for block
        ClearBlock(key);
        // update info of returning block
        SetBlock(TKey(size, key.address));
        // update info of leftover block
        size_t loSize = key.size - size;
        char* loBlock = key.address + size;
        TKey k(loSize, loBlock);
        SetBlock(k);
        // then insert leftover block in tree
        bool res = sTree.Insert(k);
        (void)res; // suppress warning
        // insert can't fail, we own the block
        ASSERT(res);
    }

    // return block
    ASSERT(((size_t)key.address & PAGE_MASK) == 0);
    return key.address;
}

void FreeBlock(TKey key)
{
    ASSERT((key.size & PAGE_MASK) == 0);
    ASSERT(((size_t)key.address & PAGE_MASK) == 0);

    // update page map before coalescing
    // @todo: optimize, this is useless if we don't coalesce at all
    ClearBlock(key);

    // try backwards coalescing
    while (true)
    {
        char* prevPage = (char*)(key.address - PAGE);
        PageInfo info = GetPageInfoForPtr(prevPage);

        // fetch info for previous block
        if (info.size == 0)
            break; // no info for previous block

        TKey k(-info.size, (char*)(key.address + info.size));
        if (info.size == PAGE) // single-page block
            k = TKey(PAGE, (char*)(key.address - PAGE));

        // try to acquire previous block
        // can fail if: block not free, or does not exist
        if (!sTree.Remove(k))
            break;

        // backward coalescing successful
        // update page map for found block
        ClearBlock(k);
        // update block size
        key.size += k.size;
        key.address = k.address;
        break;
    }

    // try forward coalescing
    while (true)
    {
        char* nextBlock = (char*)(key.address + key.size);
        PageInfo info = sPageMap.GetPageInfo(nextBlock);

        // fetch info for next block
        if (info.size <= 0)
            break;

        TKey k((size_t)info.size, nextBlock);

        // try to acquire next block
        // can fail if: block not free, or does not exist
        if (!sTree.Remove(k))
            break;

        // forward coalescing successful
        // update page map
        ClearBlock(k);
        // update block size
        key.size += k.size;
        break;
    }

    // update page map after coalescing
    SetBlock(key);
    // and add to tree as a free block
    bool res = sTree.Insert(key);
    (void)res; // suppress unused warning
    ASSERT(res);
}

void ReserveBlockFromOS(size_t pages)
{
    size_t const blockSize = pages * PAGE;
    char* block = (char*)PageAlloc(blockSize);
    if (UNLIKELY(block == nullptr))
        return;

    TKey key = TKey(blockSize, block);
    // update page map
    SetBlock(key);

    bool res = sTree.Insert(key);
    (void)res; // suppress warning
    // insert can't failblockSize own the block
    ASSERT(res);
}
