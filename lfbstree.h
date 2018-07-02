
#ifndef __LFBSTREE
#define __LFBSTREE

// implementation of a lock free binary search tree
//  based on the paper "Fast Concurrent Lock-Free Binary Search Trees"
//  by Aravind Natarajan and Neeraj Mittal
// specialized implementation for coalescing memory allocator

#include <atomic>
#include <new>

#include "defines.h"
#include "log.h"

// tree ordered by
// 1. block size
// 2. block address
struct TKey
{
    size_t size;
    char* address;

public:
    TKey() : size(0U), address(nullptr) { }
    TKey(size_t s) : size(s), address(nullptr){ }
    TKey(size_t s, char* addr) : size(s), address(addr) { }

    bool operator>(TKey const& other) const
    {
        if (size != other.size)
            return size > other.size;

        return address > other.address;
    }

    bool operator==(TKey const& other) const
    {
        return size == other.size && address == other.address;
    }

    bool operator!=(TKey const& other) const
    {
        return !(*this == other);
    }
};

// helper structs
struct Node;
struct NodeChild;

// node edge field
// stores pointer to node and 2 boolean values, flagged and tagged
#define NODE_CHILD_PTR_MASK (~((size_t)(1U << 3) - 1))
#define NODE_CHILD_FLAG_SHIFT 0U
#define NODE_CHILD_FLAG_MASK ((size_t)(1U << NODE_CHILD_FLAG_SHIFT))
#define NODE_CHILD_TAG_SHIFT 1U
#define NODE_CHILD_TAG_MASK ((size_t)(1U << NODE_CHILD_TAG_SHIFT))

struct NodeChild
{
public:
    NodeChild() = default;
    NodeChild(Node* node) { Init(false, false, node); }
    NodeChild(bool f, bool t, Node* node) { Init(f, t, node); }

    bool IsFlagged() const { return (bool)((size_t)_ptr & NODE_CHILD_FLAG_MASK); }
    bool IsTagged() const { return (bool)((size_t)_ptr & NODE_CHILD_TAG_MASK); }
    Node* GetPtr() const { return (Node*)((size_t)_ptr & NODE_CHILD_PTR_MASK); }

private:
    void Init(bool flagged, bool tagged, Node* ptr)
    {
        ASSERT(((size_t)ptr & ~NODE_CHILD_PTR_MASK) == 0);
        _ptr = (Node*)((size_t)ptr |
                (size_t)tagged << NODE_CHILD_TAG_SHIFT |
                (size_t)flagged << NODE_CHILD_FLAG_SHIFT);

        ASSERT(flagged == IsFlagged());
        ASSERT(tagged == IsTagged());
        ASSERT(ptr == GetPtr());
    }

private:
    // besides the pointer, 2 flags are stored in _ptr by bit-stealing
    Node* _ptr = nullptr;
};

struct Node
{
    TKey key;
    std::atomic<NodeChild> left;
    std::atomic<NodeChild> right;

public:
    Node() = default;
    Node(TKey k) : key(k) { }
};

struct SeekRecord
{
    // ancestor -> successor edge
    std::atomic<NodeChild>* ancestorEdge;
    Node* successor;
    Node* parent;
    Node* leaf;
    // needed for RemoveNext operation
    TKey lastLeftKey;
};

class LFBSTree
{
public:
    LFBSTree();
    ~LFBSTree();

    // available operations
    bool Insert(TKey key);
    bool Remove(TKey key);
    // removes smallest key from tree that is >= key
    // removed key stored in provided arg
    bool RemoveNext(TKey& key);

private:
    SeekRecord Seek(TKey key);
    bool Cleanup(TKey key, SeekRecord& record);

private:
    // default dummy nodes
    Node* _R;
    Node* _S;
};

#endif // __LFBSTREE

