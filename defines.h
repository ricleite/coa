/*
 * Copyright (C) 2019 Ricardo Leite. All rights reserved.
 * Licenced under the MIT licence. See COPYING file in the project root for details.
 */

#ifndef __DEFINES_H__
#define __DEFINES_H__

#include <cstddef> // for size_t
#include <cinttypes>

// a cache line is 64 bytes
#define LG_CACHELINE    6
// a page is 4KB
#define LG_PAGE         12
// a huge page is 2MB
#define LG_HUGEPAGE     21

#define CACHELINE   ((size_t)(1U << LG_CACHELINE))
#define PAGE        ((size_t)(1U << LG_PAGE))
#define HUGEPAGE    ((size_t)(1U << LG_HUGEPAGE))

#define CACHELINE_MASK  (CACHELINE - 1)
#define PAGE_MASK       (PAGE - 1)

// minimum alignment requirement all allocations must meet
// "address returned by malloc will be suitably aligned to store any kind of variable"
#define MIN_ALIGN sizeof(void*)

// returns smallest address >= addr with alignment align
#define ALIGN_ADDR(addr, align) \
    ( __typeof__ (addr))(((size_t)(addr) + (align - 1)) & ((~(align)) + 1))


// return smallest page size multiple that is >= s
#define PAGE_CEILING(s) \
    (((s) + (PAGE - 1)) & ~(PAGE - 1))

// https://stackoverflow.com/questions/109710/how-do-the-likely-and-unlikely-macros-in-the-linux-kernel-work-and-what-is-t
#define LIKELY(x)       __builtin_expect((x), 1)
#define UNLIKELY(x)     __builtin_expect((x), 0)

#define CMALLOC_ATTR(s) __attribute__((s))
#define CMALLOC_ALLOC_SIZE(s) CMALLOC_ATTR(alloc_size(s))
#define CMALLOC_ALLOC_SIZE2(s1, s2) CMALLOC_ATTR(alloc_size(s1, s2))
#define CMALLOC_EXPORT CMALLOC_ATTR(visibility("default"))
#define CMALLOC_NOTHROW CMALLOC_ATTR(nothrow)

#if defined(__GNUC__)
#define CMALLOC_INLINE CMALLOC_ATTR(always_inline) inline static
#elif defined(_MSC_VER)
#define CMALLOC_INLINE __forceinline inline static
#else
#define CMALLOC_INLINE
#endif

// use initial exec tls model, faster than regular tls
//  with the downside that the malloc lib can no longer be dlopen'd
// https://www.ibm.com/support/knowledgecenter/en/SSVUN6_1.1.0/com.ibm.xlcpp11.zlinux.doc/language_ref/attr_tls_model.html 
#define CMALLOC_TLS_INIT_EXEC CMALLOC_ATTR(tls_model("initial-exec"))

#define CMALLOC_CACHE_ALIGNED CMALLOC_ATTR(aligned(CACHELINE))

#define CMALLOC_CACHE_ALIGNED_FN CMALLOC_ATTR(aligned(CACHELINE))

#define STATIC_ASSERT(x, m) static_assert(x, m)

#endif // __DEFINES_H__
