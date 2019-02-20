
#include "coa.h"

#include "internal.h"
#include "log.h"

void coa_init(size_t initialSize /*= 0*/)
{
    LOG_DEBUG();

    // init page map
    sPageMap.Init();
    // init block tree
    sTree = LFBSTree();

    if (initialSize > 0)
        ReserveBlockFromOS(initialSize);
}

void* coa_alloc(size_t size)
{
    LOG_DEBUG("size: %lu", size);

    size_t pages = PAGE_CEILING(size);
    char* ptr = AllocBlock(pages);

    LOG_DEBUG("ptr: %p", ptr);
    return (void*)ptr;
}

void* coa_alloc_pages(size_t pages)
{
    LOG_DEBUG("pages: %lu", pages);

    size_t size = pages * PAGE;
    char* ptr = AllocBlock(size);

    LOG_DEBUG("ptr: %p", ptr);
    return (void*)ptr;
}

void coa_free(void* ptr)
{
    LOG_DEBUG("ptr: %p", ptr);
    if (UNLIKELY(!ptr))
        return;

    PageInfo info = GetPageInfoForPtr((char*)ptr);
    ASSERT(info.size > 0);

    TKey key(info.size, (char*)ptr);
    FreeBlock(key);
}
