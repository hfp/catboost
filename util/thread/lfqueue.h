#pragma once

#include "fwd.h"

#include <util/generic/ptr.h>
#include <util/system/atomic.h>
#include <util/system/yassert.h>
#include "lfstack.h"

#if !defined(LIST_INVERTOR_TLS_POD) && 0
# include <util/system/tls.h>
# define LIST_INVERTOR_TLS_POD
# define LIST_INVERTOR_GET(A) TlsRef(A)
#elif !defined(LIST_INVERTOR_TLS) && 0
# include <util/system/tls.h>
# define LIST_INVERTOR_TLS
# define LIST_INVERTOR_GET(A) TlsRef(A)
#else
# define LIST_INVERTOR_GET(A) A
#endif

struct TDefaultLFCounter {
    template <class T>
    void IncCount(const T& data) {
        (void)data;
    }
    template <class T>
    void DecCount(const T& data) {
        (void)data;
    }
};

template <class T, class TCounter>
class TLockFreeQueue: public TNonCopyable {
    struct TListNode {
        template <typename U>
        TListNode(U&& u, TListNode* next)
            : Next(next)
            , Data(std::forward<U>(u))
        {
        }

        template <typename U>
        explicit TListNode(U&& u)
            : Data(std::forward<U>(u))
        {
        }

        TListNode* volatile Next;
        T Data;
    };

    // using inheritance to be able to use 0 bytes for TCounter when we don't need one
    struct TRootNode: public TCounter {
        TListNode* volatile PushQueue;
        TListNode* volatile PopQueue;
        TListNode* volatile ToDelete;
        TRootNode* volatile NextFree;

        TRootNode()
            : PushQueue(nullptr)
            , PopQueue(nullptr)
            , ToDelete(nullptr)
            , NextFree(nullptr)
        {
        }
        void CopyCounter(TRootNode* x) {
            *(TCounter*)this = *(TCounter*)x;
        }
    };

    static void EraseList(TListNode* n) {
        while (n) {
            TListNode* keepNext = AtomicGet(n->Next);
            delete n;
            n = keepNext;
        }
    }

    TRootNode* volatile JobQueue;
    volatile TAtomic FreememCounter;
    volatile TAtomic FreeingTaskCounter;
    TRootNode* volatile FreePtr;

    void TryToFreeAsyncMemory() {
        TRootNode* current = AtomicGet(FreePtr);
        if (current == nullptr)
            return;
        if (AtomicGet(FreememCounter) == 1) {
            TAtomic keepCounter = AtomicGet(FreeingTaskCounter);
            // we are the last thread, try to cleanup
            // check if another thread have cleaned up
            if (keepCounter != AtomicGet(FreeingTaskCounter)) {
                return;
            }
            if (AtomicCas(&FreePtr, (TRootNode*)nullptr, current)) {
                // free list
                while (current) {
                    TRootNode* p = AtomicGet(current->NextFree);
                    EraseList(AtomicGet(current->ToDelete));
                    delete current;
                    current = p;
                }
                AtomicAdd(FreeingTaskCounter, 1);
            }
        }
    }
    void AsyncRef() {
        AtomicAdd(FreememCounter, 1);
    }
    void AsyncUnref() {
        TryToFreeAsyncMemory();
        AtomicSub(FreememCounter, 1);
    }
    void AsyncDel(TRootNode* toDelete, TListNode* lst) {
        AtomicSet(toDelete->ToDelete, lst);
        for (;;) {
            AtomicSet(toDelete->NextFree, AtomicGet(FreePtr));
            if (AtomicCas(&FreePtr, toDelete, AtomicGet(toDelete->NextFree)))
                break;
        }
    }
    void AsyncUnref(TRootNode* toDelete, TListNode* lst) {
        TryToFreeAsyncMemory();
        if (AtomicSub(FreememCounter, 1) == 0) {
            // no other operations in progress, can safely reclaim memory
            EraseList(lst);
            delete toDelete;
        } else {
            // Dequeue()s in progress, put node to free list
            AsyncDel(toDelete, lst);
        }
    }

    struct TListInvertor {
        TListNode* Copy;
        TListNode* Tail;
        TListNode* PrevFirst;
#if defined(LIST_INVERTOR_TLS_POD) || defined(LIST_INVERTOR_TLS)
        TListNode* Recycle;
#endif
#if !defined(LIST_INVERTOR_TLS_POD)
        TListInvertor()
            : Copy(nullptr)
            , Tail(nullptr)
            , PrevFirst(nullptr)
# if defined(LIST_INVERTOR_TLS)
            , Recycle(nullptr)
# endif
        {
        }
        ~TListInvertor() {
            EraseList(Copy);
# if defined(LIST_INVERTOR_TLS)
            EraseList(Recycle);
# endif
        }
#endif
        void CopyWasUsed() {
            Copy = nullptr;
            Tail = nullptr;
            PrevFirst = nullptr;
        }
        void DoCopy(TListNode* ptr) {
            TListNode* newFirst = ptr;
            TListNode* newCopy = nullptr;
            TListNode* newTail = nullptr;
            while (ptr) {
                if (ptr == PrevFirst) {
                    // short cut, we have copied this part already
                    AtomicSet(Tail->Next, newCopy);
                    newCopy = Copy;
                    Copy = nullptr; // do not destroy prev try
                    if (!newTail)
                        newTail = Tail; // tried to invert same list
                    break;
                }
                TListNode* newElem;
#if defined(LIST_INVERTOR_TLS_POD) || defined(LIST_INVERTOR_TLS)
                if (Recycle) {
                    newElem = Recycle;
                    Recycle = Recycle->Next;
                    newElem->Data = ptr->Data;
                    newElem->Next = newCopy;
                }
                else
#endif
                {
                    newElem = new TListNode(ptr->Data, newCopy);
                }
                newCopy = newElem;
                ptr = AtomicGet(ptr->Next);
                if (!newTail)
                    newTail = newElem;
            }
#if defined(LIST_INVERTOR_TLS_POD) || defined(LIST_INVERTOR_TLS)
            Recycle = Copy; // copy was useless
#else
            EraseList(Copy); // copy was useless
#endif
            Copy = newCopy;
            PrevFirst = newFirst;
            Tail = newTail;
        }
    };

    void EnqueueImpl(TListNode* head, TListNode* tail) {
        TRootNode* newRoot = new TRootNode;
        AsyncRef();
        AtomicSet(newRoot->PushQueue, head);
        for (;;) {
            TRootNode* curRoot = AtomicGet(JobQueue);
            AtomicSet(newRoot->PushQueue, head);
            AtomicSet(tail->Next, AtomicGet(curRoot->PushQueue));
            AtomicSet(newRoot->PopQueue, AtomicGet(curRoot->PopQueue));
            newRoot->CopyCounter(curRoot);

            for (TListNode* node = head;; node = AtomicGet(node->Next)) {
                newRoot->IncCount(node->Data);
                if (node == tail)
                    break;
            }

            if (AtomicCas(&JobQueue, newRoot, curRoot)) {
                AsyncUnref(curRoot, nullptr);
                break;
            }
        }
    }

public:
    TLockFreeQueue()
        : JobQueue(new TRootNode)
        , FreememCounter(0)
        , FreeingTaskCounter(0)
        , FreePtr(nullptr)
    {
    }
    ~TLockFreeQueue() {
        AsyncRef();
        AsyncUnref(); // should free FreeList
        EraseList(JobQueue->PushQueue);
        EraseList(JobQueue->PopQueue);
        delete JobQueue;
    }
    template <typename U>
    void Enqueue(U&& data) {
        TListNode* newNode = new TListNode(std::forward<U>(data));
        EnqueueImpl(newNode, newNode);
    }
    void Enqueue(T&& data) {
        TListNode* newNode = new TListNode(std::move(data));
        EnqueueImpl(newNode, newNode);
    }
    void Enqueue(const T& data) {
        TListNode* newNode = new TListNode(data);
        EnqueueImpl(newNode, newNode);
    }
    template <typename TCollection>
    void EnqueueAll(const TCollection& data) {
        EnqueueAll(data.begin(), data.end());
    }
    template <typename TIter>
    void EnqueueAll(TIter dataBegin, TIter dataEnd) {
        if (dataBegin == dataEnd)
            return;

        TIter i = dataBegin;
        TListNode* volatile node = new TListNode(*i);
        TListNode* volatile tail = node;

        for (++i; i != dataEnd; ++i) {
            TListNode* nextNode = node;
            node = new TListNode(*i, nextNode);
        }
        EnqueueImpl(node, tail);
    }
    bool Dequeue(T* data) {
        TRootNode* newRoot = nullptr;
#if defined(LIST_INVERTOR_TLS_POD)
        Y_POD_STATIC_THREAD(TListInvertor) listInvertor;
        LIST_INVERTOR_GET(listInvertor).CopyWasUsed();
#elif defined(LIST_INVERTOR_TLS)
        Y_STATIC_THREAD(TListInvertor) listInvertor;
        LIST_INVERTOR_GET(listInvertor).CopyWasUsed();
#else
        TListInvertor listInvertor;
#endif
        AsyncRef();
        for (;;) {
            TRootNode* curRoot = AtomicGet(JobQueue);
            TListNode* tail = AtomicGet(curRoot->PopQueue);
            if (tail) {
                // has elems to pop
                if (!newRoot)
                    newRoot = new TRootNode;

                AtomicSet(newRoot->PushQueue, AtomicGet(curRoot->PushQueue));
                AtomicSet(newRoot->PopQueue, AtomicGet(tail->Next));
                newRoot->CopyCounter(curRoot);
                newRoot->DecCount(tail->Data);
                Y_ASSERT(AtomicGet(curRoot->PopQueue) == tail);
                if (AtomicCas(&JobQueue, newRoot, curRoot)) {
                    *data = std::move(tail->Data);
                    AtomicSet(tail->Next, nullptr);
                    AsyncUnref(curRoot, tail);
                    return true;
                }
                continue;
            }
            if (AtomicGet(curRoot->PushQueue) == nullptr) {
                delete newRoot;
                AsyncUnref();
                return false; // no elems to pop
            }

            if (!newRoot)
                newRoot = new TRootNode;
            AtomicSet(newRoot->PushQueue, nullptr);
            LIST_INVERTOR_GET(listInvertor).DoCopy(AtomicGet(curRoot->PushQueue));
            newRoot->PopQueue = LIST_INVERTOR_GET(listInvertor).Copy;
            newRoot->CopyCounter(curRoot);
            Y_ASSERT(AtomicGet(curRoot->PopQueue) == nullptr);
            if (AtomicCas(&JobQueue, newRoot, curRoot)) {
                newRoot = nullptr;
                LIST_INVERTOR_GET(listInvertor).CopyWasUsed();
                AsyncDel(curRoot, AtomicGet(curRoot->PushQueue));
            } else {
                AtomicSet(newRoot->PopQueue, nullptr);
            }
        }
    }
    bool IsEmpty() {
        AsyncRef();
        TRootNode* curRoot = AtomicGet(JobQueue);
        bool res = AtomicGet(curRoot->PushQueue) == nullptr && AtomicGet(curRoot->PopQueue) == nullptr;
        AsyncUnref();
        return res;
    }
    TCounter GetCounter() {
        AsyncRef();
        TRootNode* curRoot = AtomicGet(JobQueue);
        TCounter res = *(TCounter*)curRoot;
        AsyncUnref();
        return res;
    }
};

template <class T, class TCounter>
class TAutoLockFreeQueue {
public:
    using TRef = TAutoPtr<T>;

    inline ~TAutoLockFreeQueue() {
        TRef tmp;

        while (Dequeue(&tmp)) {
        }
    }

    inline bool Dequeue(TRef* t) {
        T* res = nullptr;

        if (Queue.Dequeue(&res)) {
            t->Reset(res);

            return true;
        }

        return false;
    }

    inline void Enqueue(TRef& t) {
        Queue.Enqueue(t.Get());
        Y_UNUSED(t.Release());
    }

    inline void Enqueue(TRef&& t) {
        Queue.Enqueue(t.Get());
        Y_UNUSED(t.Release());
    }

    inline bool IsEmpty() {
        return Queue.IsEmpty();
    }

    inline TCounter GetCounter() {
        return Queue.GetCounter();
    }

private:
    TLockFreeQueue<T*, TCounter> Queue;
};
