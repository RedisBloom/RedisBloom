/*
* Copyright 2019 Redis Labs Ltd. and Contributors
*
* This file is available under the Redis Labs Source Available License Agreement
*/
#include <assert.h>     // assert
#include <math.h>       // q, ceil
#include <stdio.h>      // printf
#include <stdlib.h>     // malloc
#include <stdbool.h>    // bool

#include "topk.h"
#include "../contrib/murmurhash2.h"

#define TOPK_HASH(item, itemlen, i) MurmurHash2(item, itemlen, i)
#define GA 1919

/* Byte-wise swap two items of size SIZE. */
#define SWAP(a, b, size)                  \
  do {                                    \
    register size_t __size = (size);      \
    register char *__a = (a), *__b = (b); \
    do {                                  \
      char __tmp = *__a;                  \
      *__a++ = *__b;                      \
      *__b++ = __tmp;                     \
    } while (--__size > 0);               \
  } while (0)

typedef uint32_t counter_t;

struct Bucket {
    uint32_t fp;        //  fingerprint
    counter_t count;
};

struct HeapBucket {
    uint32_t fp;
    uint32_t itemlen;
    char *item;
    counter_t count;
};

static inline uint32_t max(uint32_t a, uint32_t b) {
  return a > b ? a : b;
}

void heapifyDown(HeapBucket *array, size_t len, size_t start) {
    // left-child of __start is at 2 * __start + 1
    // right-child of __start is at 2 * __start + 2
    size_t child = start;

    if (len < 2 || (len - 2) / 2 < child) return;

    child = 2 * child + 1;

    if ((child + 1) < len &&
        (array[child].count > array[child + 1].count)) { // changed sign min heap
        // right-child exists and is greater than left-child
        ++child;
    }

    // check if we are in heap-order
    if (array[child].count > array[start].count)
        // we are, __start is larger than it's largest child
        return;

    HeapBucket top = { 0 };
    memcpy(&top, &array[start], sizeof(HeapBucket));
    do {
        // we are not in heap-order, swap the parent with it's largest child
        memcpy(&array[start], &array[child], sizeof(HeapBucket));
        start = child;

        if ((len - 2) / 2 < child) break;

        // recompute the child based off of the updated parent
        child = 2 * child + 1;

        if ((child + 1) < len &&
            (array[child].count > array[child + 1].count)) {  // changed sign min heap
            // right-child exists and is greater than left-child
            ++child;
        }

        // check if we are in heap-order
    } while (array[child].count < top.count);
    memcpy(&array[start], &top, sizeof(HeapBucket));
}

void heapifyUp(HeapBucket *array, size_t len, size_t start) {
    // TODO: for query
}

TopK *TopK_Create(uint32_t k, uint32_t width, uint32_t depth) {
    assert(k > 0);
    assert(width > 0);
    assert(depth > 0);
    
    TopK *topk = (TopK *)TOPK_CALLOC(1, sizeof(topk));
    topk->k = k;
    topk->width = width;
    topk->depth = depth;
    topk->data = (Bucket *)TOPK_CALLOC(width * depth, sizeof(Bucket));
    topk->heap = TOPK_CALLOC(k, sizeof(HeapBucket));

    return topk;
}

void TopK_Destroy(TopK *topk) {
    assert(topk);
 
    free(topk->heap);
    topk->heap = NULL;
    free(topk->data);
    topk->data = NULL;
    free(topk);
}

// Complexity O(k + strlen)
static HeapBucket *checkExistInHeap(TopK *topk, const char *item, size_t itemlen) {
    uint32_t fp = TOPK_HASH(item, itemlen, GA);
    HeapBucket *runner = topk->heap;
 
    for(int i = topk->k - 1; i >= 0; --i) 
        if(fp == (runner + i)->fp && itemlen == (runner + i)->itemlen && 
                    memcmp((runner + i)->item, item, itemlen) == 0) {
            return runner + i;     
        }
    return NULL;
}

void TopK_Add(TopK *topk, const char *item, size_t itemlen) {
    assert(topk);
    assert(item);
    assert(itemlen);

    counter_t count = 0, maxv = 0;
    counter_t heapMin = topk->heap->count;
    uint32_t fp = TOPK_HASH(item, itemlen, GA);
    HeapBucket *itemHeapPtr = checkExistInHeap(topk, item, itemlen);
    Bucket *runner = NULL;

    for(int i = 0; i < topk->depth; ++i) {
        uint32_t loc = TOPK_HASH(item, itemlen, i) % topk->width;
        runner = topk->data + i * topk->width + loc;
        count = runner->count;
        if(count == 0) {
            runner->fp = fp;
            runner->count = 1;
            maxv = max(maxv, runner->count);
        } else if(runner->fp == fp) {
            if(itemHeapPtr || count < heapMin) {
                ++runner->count;
                maxv = max(maxv, runner->count); // might be combined with next max call
                                                 // or might need to max even when <heapMin   
            }
        } else {
            double decay = pow(DECAY, -count);
            double chance = rand() / (double)RAND_MAX;
            if(chance < decay) {
                --runner->count;
                if(runner->count == 0) {
                    runner->fp = fp;
                    runner->count = 1;
                }
            }
            maxv = max(maxv, runner->count);
        }
    }
    if(itemHeapPtr != NULL) {
        //itemHeapPtr->count = max(itemHeapPtr->count, maxv);
        printf("Exist in heap\n");
        itemHeapPtr->count = maxv;
        heapifyDown(topk->heap, topk->k, itemHeapPtr - topk->heap);
    } else {
        if(maxv > heapMin) {
            printf("Doesn't exist in heap\n");
            topk->heap->count = maxv;
            topk->heap->fp = fp;
            topk->heap->item = (char *)item;
            topk->heap->itemlen = itemlen;
            heapifyDown(topk->heap, topk->k, 0);
        }
    }
}

bool TopK_Query(TopK *topk, const char *item, size_t itemlen) {
    return checkExistInHeap(topk, item, itemlen) != NULL;
}

size_t TopK_Count(TopK *topk, const char *item, size_t itemlen) {
    assert(topk);
    assert(item);
    assert(itemlen);

    Bucket *runner = NULL;
    uint32_t fp = TOPK_HASH(item, itemlen, GA);
    counter_t res = 0;

    for(int i = 0; i < topk->depth; ++i) {
        uint32_t loc = TOPK_HASH(item, itemlen, i) % topk->width;
        runner = topk->data + i * topk->width + loc;
        if(runner->fp == fp)
        {
            res = max(res, runner->count);
        }
    }
    return res;
}

const char **TopK_List(TopK *topk) {
    const char **ret = TOPK_CALLOC(topk->k, (sizeof(char *)));
    for(uint32_t i = 0; i < topk->k; ++i) {
        ret[i] = topk->heap[i].item;
    }
    return ret;
}