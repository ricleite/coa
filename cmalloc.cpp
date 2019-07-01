/*
 * Copyright (C) 2019 Ricardo Leite. All rights reserved.
 * Licenced under the MIT licence. See COPYING file in the project root for details.
 */

#include <cstring> // for memset/memcpy

// for ENOMEM
#include <errno.h>

#include "cmalloc.h"
#include "internal.h"
#include "log.h"

// global variables
// malloc init state
bool MallocInit = false;

void InitMalloc()
{
    LOG_DEBUG();

    // hard assumption that this can't be called concurrently
    MallocInit = true;

    // init page map
    sPageMap.Init();

    // init block tree
    sTree = LFBSTree();
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

