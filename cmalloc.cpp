
#include <cassert>
#include <cstring>
#include <algorithm>
#include <atomic>

// for ENOMEM
#include <errno.h>

#include "cmalloc.h"
#include "pages.h"
#include "pagemap.h"
#include "log.h"

#include "lfbstree.h"

// global variables
// malloc init state
bool MallocInit = false;
// block tree
LFBSTree Tree;

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

PageInfo GetPageInfoForPtr(char* ptr)
{
    return sPageMap.GetPageInfo(ptr);
}

char* AllocBlock(size_t size)
{
    if (UNLIKELY(size == 0))
        size = PAGE;

    ASSERT((size & PAGE_MASK) == 0);

    TKey key(size);
    if (!Tree.RemoveNext(key))
    {
        // no available blocks
        // alloc a large block and carve from it
        size_t blockSize = std::max(size, HUGEPAGE);
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
        bool res = Tree.Insert(k);
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
        if (!Tree.Remove(k))
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
        if (!Tree.Remove(k))
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
    bool res = Tree.Insert(key);
    (void)res; // suppress unused warning
    ASSERT(res);
}

void InitMalloc()
{
    LOG_DEBUG();

    // hard assumption that this can't be called concurrently
    MallocInit = true;

    // init page map
    sPageMap.Init();

    // init block tree
    Tree = LFBSTree();
}

void c_malloc_initialize() { }

void c_malloc_finalize() { }

void c_malloc_thread_initialize() { }

void c_malloc_thread_finalize() { }

extern "C"
void* c_malloc(size_t size) noexcept
{
    LOG_DEBUG("size: %lu", size);

    // ensure malloc is initialized
    if (UNLIKELY(!MallocInit))
        InitMalloc();

    // large block allocation
    size_t pages = PAGE_CEILING(size);
    char* ptr = AllocBlock(pages);
    LOG_DEBUG("ptr: %p", ptr);
    return (void*)ptr;
}

extern "C"
void* c_calloc(size_t n, size_t size) noexcept
{
    LOG_DEBUG();
    size_t allocSize = n * size;
    // overflow check
    // @todo: expensive, need to optimize
    if (UNLIKELY(n == 0 || allocSize / n != size))
        return nullptr;

    void* ptr = c_malloc(allocSize);

    // calloc returns zero-filled memory
    // @todo: optimize, memory may be already zero-filled 
    //  if coming directly from OS
    if (LIKELY(ptr != nullptr))
        memset(ptr, 0x0, allocSize);

    return ptr;
}

extern "C"
void* c_realloc(void* ptr, size_t size) noexcept
{
    LOG_DEBUG();

    size_t blockSize = 0;
    if (LIKELY(ptr != nullptr))
    {
        PageInfo info = GetPageInfoForPtr((char*)ptr);
        ASSERT(info.size > 0);
        blockSize = info.size;

        // realloc with size == 0 is the same as free(ptr)
        if (UNLIKELY(size == 0))
        {
            c_free(ptr);
            return nullptr;
        }

        // nothing to do, block is already large enough
        if (UNLIKELY(size <= blockSize))
            return ptr;
    }

    void* newPtr = c_malloc(size);
    if (LIKELY(ptr && newPtr))
    {
        memcpy(newPtr, ptr, blockSize);
        c_free(ptr);
    }

    return newPtr;
}

extern "C"
size_t c_malloc_usable_size(void* ptr) noexcept
{
    LOG_DEBUG();
    if (UNLIKELY(ptr == nullptr))
        return 0;

    PageInfo info = GetPageInfoForPtr((char*)ptr);
    ASSERT(info.size > 0);
    return size_t(info.size);
}

extern "C"
int c_posix_memalign(void** memptr, size_t alignment, size_t size) noexcept
{
    LOG_DEBUG();

    // @todo: accept arbitrary alignment
    // right now, we're relying on the fact that all allocations start
    // at the beginning of pages 
    ASSERT(alignment <= PAGE);

    char* ptr = (char*)c_malloc(size);
    if (!ptr)
        return ENOMEM;
    
    LOG_DEBUG("provided ptr: %p", ptr);
    *memptr = ptr;
    return 0;
}

extern "C"
void* c_aligned_alloc(size_t alignment, size_t size) noexcept
{
    LOG_DEBUG();
    void* ptr = nullptr;
    int ret = c_posix_memalign(&ptr, alignment, size);
    if (ret)
        return nullptr;

    return ptr;
}

extern "C"
void* c_valloc(size_t size) noexcept
{
    LOG_DEBUG();
    return c_aligned_alloc(PAGE, size);
}

extern "C"
void* c_memalign(size_t alignment, size_t size) noexcept
{
    LOG_DEBUG();
    return c_aligned_alloc(alignment, size);
}

extern "C"
void* c_pvalloc(size_t size) noexcept
{
    LOG_DEBUG();
    size = ALIGN_ADDR(size, PAGE);
    return c_aligned_alloc(PAGE, size);
}

extern "C"
void c_free(void* ptr) noexcept
{
    LOG_DEBUG("ptr: %p", ptr);
    if (UNLIKELY(!ptr))
        return;

    PageInfo info = GetPageInfoForPtr((char*)ptr);
    ASSERT(info.size > 0);

    TKey key(info.size, (char*)ptr);
    FreeBlock(key);
}

