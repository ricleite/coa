
#include <limits>
#include <utility>
#include "lfbstree.h"
#include "pages.h"
#include "log.h"

// internal memory allocation helpers
Node* AllocNode();
void RetireNode(Node* node);

// global variables
// static std::atomic<char*> HeadNode(nullptr);
static __thread char* HeadNode = nullptr;

// forward arguments
template<class T>
Node* AllocNode(T&& arg)
{
    // size of page blocks to carve up nodes from
    size_t const blockSize = HUGEPAGE;

    while (true)
    {
        // char* head = HeadNode.load();
        char* head = HeadNode;
        if (head == nullptr)
        {
            // pages are 0-filled
            char* buffer = (char*)PageAlloc(blockSize);
            // carve up buffer into a node list
            size_t numNodes = blockSize / sizeof(Node);
            for (size_t i = 0; i < numNodes - 1; ++i)
            {
                char* node = buffer + i * sizeof(Node);
                char* next = buffer + (i + 1) * sizeof(Node);
                *((char**)node) = (char*)next;
            }

            *(char**)(buffer + (numNodes - 1) * sizeof(Node)) = nullptr;
            HeadNode = buffer;
            /*
            if (!HeadNode.compare_exchange_strong(head, buffer))
                PageFree(buffer, blockSize);
            */

            continue;
        }

        char* next = *((char**)head);
        HeadNode = next;
        Node* node = new (head) Node(arg);
        return node;

        /*
        if (HeadNode.compare_exchange_weak(head, next))
        {
            // https://stackoverflow.com/questions/519808/call-a-constructor-on-a-already-allocated-memory
            Node* node = new (head) Node(arg);
            return node;
        }
        */
    }
}

void RetireNode(Node* /*node*/) { /* no-op */ }

// `old` subtree is no longer reachable and is being removed from tree
// it was replaced by `existing`, which is a descendant of `old`
void RetireSubtree(Node* old, Node* existing)
{
    RetireNode(old);
    NodeChild left = old->left.load();
    NodeChild right = old->right.load();
    Node* leftNode = left.GetPtr();
    Node* rightNode = right.GetPtr();
    // leaf nodes have no children
    // internal nodes have both children
    ASSERT((bool)leftNode == (bool)rightNode);
    if (leftNode)
    {
        ASSERT(left.IsFlagged() || left.IsTagged());
        if (leftNode == existing)
            ASSERT(left.IsTagged());
        else
            RetireSubtree(leftNode, existing);
    }

    if (rightNode)
    {
        ASSERT(right.IsFlagged() || right.IsTagged());
        if (rightNode == existing)
            ASSERT(right.IsTagged());
        else
            RetireSubtree(rightNode, existing);
    }
}

LFBSTree::LFBSTree()
{
    // assemble initial tree structure
    //          R           //
    //        /   \         //
    //      S     oo2       //
    //    /   \             //
    //  oo0   oo1           //
    // R has initial value 

    auto limits = std::numeric_limits<size_t>();
    TKey const oo2 = TKey(limits.max() - 0U);
    TKey const oo1 = TKey(limits.max() - 1U);
    TKey const oo0 = TKey(limits.max() - 2U);

    // allocate R and S
    // nodes for oo1 and oo2 aren't actually necessarily
    // but include them for the sake of later sanity checks
    _R = AllocNode(oo2);
    _S = AllocNode(oo1);
    // init R
    _R->left.store(NodeChild(_S));
    _R->right.store(NodeChild(AllocNode(oo2)));
    // init S
    _S->left.store(NodeChild(AllocNode(oo0)));
    _S->right.store(NodeChild(AllocNode(oo1)));

    ASSERT(_R);
    ASSERT(_S);
}

LFBSTree::~LFBSTree() { }

SeekRecord LFBSTree::Seek(TKey key)
{
    std::atomic<NodeChild>* ancestorEdge = &_R->left;
    Node* successor = _S;
    Node* parent = _S;
    Node* leaf = _S->left.load().GetPtr();
    // needed for RemoveNext operation
    TKey lastLeftKey = _R->key;

    ASSERT(leaf);

    std::atomic<NodeChild>* parentEdgePtr = &parent->left;
    std::atomic<NodeChild>* leafEdgePtr = &leaf->left;
    NodeChild parentEdge = parentEdgePtr->load();
    NodeChild leafEdge = leafEdgePtr->load();

    Node* curr = leafEdge.GetPtr();
    while (curr != nullptr)
    {
        // leafEdge/parentEdge contain known addresses
        ASSERT((size_t)parent + offsetof(Node, left) == (size_t)parentEdgePtr ||
               (size_t)parent + offsetof(Node, right) == (size_t)parentEdgePtr);
        ASSERT((size_t)leaf + offsetof(Node, left) == (size_t)leafEdgePtr ||
               (size_t)leaf + offsetof(Node, right) == (size_t)leafEdgePtr);

        // parent is an internal node
        // and internal nodes always have both childs
        NodeChild leftChild = parent->left.load();
        NodeChild rightChild = parent->right.load();
        (void)leftChild;
        (void)rightChild; // suppress unused warning
        // parent is an internal node and must have 2 child ptrs
        ASSERT(leftChild.GetPtr() && rightChild.GetPtr());

        // update ancestor/successor if leaf isn't tagged for removal
        // leaf is an internal node, can't be flagged
        ASSERT(!parentEdge.IsFlagged() || parentEdge.GetPtr() != leaf);
        if (!parentEdge.IsTagged())
        {
            ancestorEdge = parentEdgePtr;
            successor = leaf;
        }

        // update parent/leaf
        parent = leaf;
        leaf = curr;

        // and parentEdge/leafEdge
        parentEdgePtr = leafEdgePtr;
        parentEdge = leafEdge;
        if (leaf->key > key)
        {
            lastLeftKey = leaf->key;
            leafEdgePtr = &leaf->left;
        }
        else
            leafEdgePtr = &leaf->right;

        leafEdge = leafEdgePtr->load();
        // update curr
        curr = leafEdge.GetPtr();

        ASSERT(!curr || (leaf->key > curr->key) == (leafEdgePtr == &leaf->left));
    }

    SeekRecord record;
    record.ancestorEdge = ancestorEdge;
    record.successor = successor;
    record.parent = parent;
    record.leaf = leaf;
    record.lastLeftKey = lastLeftKey;
    return record;
}

bool LFBSTree::Insert(TKey key)
{
    while (true)
    {
        SeekRecord record = Seek(key);
        Node* leaf = record.leaf;
        // key already in tree
        if (leaf->key == key)
        {
            ASSERT(false);
            return false;
        }

        Node* newLeaf = AllocNode(key);
        Node* newInternal = AllocNode(key);
        if (leaf->key > key)
        {
            newInternal->key = leaf->key; // update key
            newInternal->left.store(newLeaf);
            newInternal->right.store(leaf);
        }
        else
        {
            newInternal->left.store(leaf);
            newInternal->right.store(newLeaf);
        }

        ASSERT(newInternal->right.load().GetPtr()->key == newInternal->key);
        ASSERT(newInternal->key > newInternal->left.load().GetPtr()->key);

        Node* parent = record.parent;
        std::atomic<NodeChild>* childAddr = (parent->key > key) ?
            &parent->left : &parent->right;

        NodeChild expected(leaf);
        NodeChild desired(newInternal);
        if (childAddr->compare_exchange_strong(expected, desired))
            return true;

        // CAS failed, either someone only added a node
        // (and/or) leaf is flagged/tagged
        // if the later, aid deletion
        if (expected.GetPtr() == leaf &&
            (expected.IsFlagged() || expected.IsTagged()))
            Cleanup(key, record);
    }
}

bool LFBSTree::Remove(TKey key)
{
    while (true)
    {
        SeekRecord record = Seek(key);
        Node* leaf = record.leaf;
        if (leaf->key != key)
            return false;

        Node* parent = record.parent;
        std::atomic<NodeChild>* parentEdge = (parent->key > key) ?
            &parent->left : &parent->right;

        NodeChild expected(leaf);
        NodeChild desired(true, false, leaf);
        if (!parentEdge->compare_exchange_weak(expected, desired))
        {
            // CAS failed, either because edge is already tagged or flagged
            // or leaf value changed
            if (expected.GetPtr() == leaf &&
                (expected.IsFlagged() || expected.IsTagged()))
                Cleanup(key, record);

            // try again
            continue;
        }

        // CAS succeeded, node is flagged
        // now need to remove it from tree
        if (Cleanup(key, record))
            return true; // successfully removed from tree

        // if cleanup failed, someone else might have removed node
        while (true)
        {
            SeekRecord newRecord = Seek(key);
            // someone else removed key
            // key might exist in tree (e.g record.leaf->key == key), but
            //  marked leaf was removed
            if (newRecord.leaf != leaf)
                return true;

            if (Cleanup(key, newRecord))
                return true; // successfully removed from tree
        }
    }
}

bool LFBSTree::RemoveNext(TKey& key)
{
    // given key, remove smallest key from tree that is >= key
    // need to be careful not to accidentally remove one of the static nodes
    auto limits = std::numeric_limits<size_t>();
    TKey oo0 = TKey(limits.max() - 2U);
    while (oo0 > key)
    {
        SeekRecord record = Seek(key);
        Node* leaf = record.leaf;
        if (leaf->key == key && Remove(key))
            return true;

        // if key not in tree, iteratively increase to parent's key
        ASSERT(record.lastLeftKey > key);
        key = record.lastLeftKey; 
    }

    return false;
}

bool LFBSTree::Cleanup(TKey key, SeekRecord& record)
{
    std::atomic<NodeChild>* ancestorEdge = record.ancestorEdge;
    Node* successor = record.successor;
    Node* parent = record.parent;
    std::atomic<NodeChild>* childAddr = (parent->key > key) ?
        &parent->left : &parent->right;
    std::atomic<NodeChild>* siblingAddr = (parent->key > key) ?
        &parent->right : &parent->left;

    // if child isn't flagged, then sibling must be flagged
    NodeChild tmp = childAddr->load();
    NodeChild smb = siblingAddr->load();
    ASSERT(tmp.IsFlagged() || smb.IsFlagged());
    if (!tmp.IsFlagged())
    {
        // std::swap(childAddr, siblingAddr);
        auto tmp = childAddr;
        childAddr = siblingAddr;
        siblingAddr = tmp;
    }

    ASSERT(childAddr != siblingAddr);
    // child must be flagged
    ASSERT(childAddr->load().IsFlagged());

    // toggle tag on sibling
    // no modify operation can happen at this edge now
    NodeChild expected = siblingAddr->load();
    // another thread can concurrently set tag
    // ASSERT(expected.IsTagged() == false);
    NodeChild desired = NodeChild(expected.IsFlagged(), true, expected.GetPtr());
    while (!siblingAddr->compare_exchange_weak(expected, desired))
        desired = NodeChild(expected.IsFlagged(), true, expected.GetPtr());

    ASSERT(expected.IsFlagged() == desired.IsFlagged());
    ASSERT(expected.GetPtr() == desired.GetPtr());

    desired = siblingAddr->load();

    // flag field must be copied to new edge
    // make sibling direct child of ancestor node
    NodeChild aExpected = NodeChild(successor);
    NodeChild aDesired = NodeChild(desired.IsFlagged(), false, desired.GetPtr());
    ASSERT(aExpected.GetPtr() != aDesired.GetPtr());
    if (ancestorEdge->compare_exchange_strong(aExpected, aDesired))
    {
        // successfully swapped sucessor subtree by sibling
        // now need to retire unreachable nodes
        RetireSubtree(successor, aDesired.GetPtr());
        return true;
    }

    return false;
}

