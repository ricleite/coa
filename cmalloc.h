/*
 * Copyright (C) 2019 Ricardo Leite. All rights reserved.
 * Licenced under the MIT licence. See COPYING file in the project root for details.
 */

#ifndef __CMALLOC_H
#define __CMALLOC_H

#include <atomic>

#include "defines.h"
#include "log.h"

#define c_malloc malloc
#define c_free free
#define c_calloc calloc
#define c_realloc realloc
#define c_malloc_usable_size malloc_usable_size
#define c_posix_memalign posix_memalign
#define c_aligned_alloc aligned_alloc
#define c_valloc valloc
#define c_memalign memalign
#define c_pvalloc pvalloc

// called on process init/exit
void c_malloc_initialize();
void c_malloc_finalize();
// called on thread enter/exit
void c_malloc_thread_initialize();
void c_malloc_thread_finalize();

// exports
extern "C"
{
    // malloc interface
    void* c_malloc(size_t size) noexcept
        CMALLOC_EXPORT CMALLOC_NOTHROW CMALLOC_ALLOC_SIZE(1)
        CMALLOC_CACHE_ALIGNED_FN;
    void c_free(void* ptr) noexcept
        CMALLOC_EXPORT CMALLOC_NOTHROW
        CMALLOC_CACHE_ALIGNED_FN;
    void* c_calloc(size_t n, size_t size) noexcept
        CMALLOC_EXPORT CMALLOC_NOTHROW CMALLOC_ALLOC_SIZE2(1, 2)
        CMALLOC_CACHE_ALIGNED_FN;
    void* c_realloc(void* ptr, size_t size) noexcept
        CMALLOC_EXPORT CMALLOC_NOTHROW CMALLOC_ALLOC_SIZE(2)
        CMALLOC_CACHE_ALIGNED_FN;
    // utilities
    size_t c_malloc_usable_size(void* ptr) noexcept;
    // memory alignment ops
    int c_posix_memalign(void** memptr, size_t alignment, size_t size) noexcept
        CMALLOC_EXPORT CMALLOC_NOTHROW CMALLOC_ATTR(nonnull(1))
        CMALLOC_CACHE_ALIGNED_FN;
    void* c_aligned_alloc(size_t alignment, size_t size) noexcept
        CMALLOC_EXPORT CMALLOC_NOTHROW CMALLOC_ALLOC_SIZE(2)
        CMALLOC_CACHE_ALIGNED_FN;
    void* c_valloc(size_t size) noexcept
        CMALLOC_EXPORT CMALLOC_NOTHROW CMALLOC_ALLOC_SIZE(1)
        CMALLOC_CACHE_ALIGNED_FN;
    // obsolete alignment oos
    void* c_memalign(size_t alignment, size_t size) noexcept
        CMALLOC_EXPORT CMALLOC_NOTHROW CMALLOC_ALLOC_SIZE(2)
        CMALLOC_CACHE_ALIGNED_FN;
    void* c_pvalloc(size_t size) noexcept
        CMALLOC_EXPORT CMALLOC_NOTHROW CMALLOC_ALLOC_SIZE(1)
        CMALLOC_CACHE_ALIGNED_FN;
}

#endif // __CMALLOC_H

