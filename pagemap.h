/*
 * Copyright (C) 2019 Ricardo Leite. All rights reserved.
 * Licenced under the MIT licence. See COPYING file in the project root for details.
 */

#ifndef __PAGEMAP_H
#define __PAGEMAP_H

#include <atomic>

#include "defines.h"
#include "log.h"

// assuming x86-64, for now
// which uses 48 bits for addressing (e.g high 16 bits ignored)
// can ignore the bottom 12 bits (lg of page)
// insignificant high bits
#define PM_NHS 12
// insignificant low bits
#define PM_NLS LG_PAGE
// significant middle bits
#define PM_SB (64 - PM_NHS - PM_NLS)
// to get the key from a address
// 1. shift to remove insignificant low bits
// 2. apply mask of middle significant bits
#define PM_KEY_SHIFT PM_NLS
#define PM_KEY_MASK ((1ULL << PM_SB) - 1)

// associates metadata to each allocator page
// implemented with a static array, but can also be implemented
//  with a multi-level radix tree

#define SC_MASK ((1ULL << 6) - 1)

// contains metadata per page
// *has* to be the size of a single word
struct PageInfo
{
    // size of block
    // if 0, page is neither start nor end of block
    // if > 0, page is start of block
    // if < 0, page is end of block
    int64_t size;

public:
    PageInfo() = default;
    PageInfo(int64_t s) : size(s) { }
};

#define PM_SZ ((1ULL << PM_SB) * sizeof(PageInfo))

static_assert(sizeof(PageInfo) == sizeof(uint64_t), "Invalid PageInfo size");

// lock free page map
class PageMap
{
public:
    // must be called before any GetPageInfo/SetPageInfo calls
    void Init();

    PageInfo GetPageInfo(char* ptr);
    void SetPageInfo(char* ptr, PageInfo info);
    // conditional update, CAS-like semantics
    bool UpdatePageInfo(char* ptr, PageInfo expected, PageInfo desired);

private:
    size_t AddrToKey(char* ptr) const;

private:
    // array based impl
    std::atomic<PageInfo>* _pagemap = { nullptr };
};

inline size_t PageMap::AddrToKey(char* ptr) const
{
    size_t key = ((size_t)ptr >> PM_KEY_SHIFT) & PM_KEY_MASK;
    return key;
}

inline PageInfo PageMap::GetPageInfo(char* ptr)
{
    size_t key = AddrToKey(ptr);
    return _pagemap[key].load();
}

inline void PageMap::SetPageInfo(char* ptr, PageInfo info)
{
    size_t key = AddrToKey(ptr);
    _pagemap[key].store(info);
}

inline bool PageMap::UpdatePageInfo(char* ptr, PageInfo expected, PageInfo desired)
{
    size_t key = AddrToKey(ptr);
    return _pagemap[key].compare_exchange_strong(expected, desired);
}

extern PageMap sPageMap;

#endif // __PAGEMAP_H

