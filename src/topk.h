/*
* Copyright 2019 Redis Labs Ltd. and Contributors
*
* This file is available under the Redis Labs Source Available License Agreement
*/
#ifndef RM_TOPK_H
#define RM_TOPK_H

#include <stdint.h>     //  uint32_t
#include <stddef.h>     //  size_t
#include <stdbool.h>    //  bool
#include <string.h>     //  memcpy
#include <stdlib.h>     //  calloc

#define REDIS_MODULE_TARGET
#ifdef REDIS_MODULE_TARGET 
#include "redismodule.h"
#define TOPK_CALLOC(count, size) RedisModule_Calloc(count, size)
#define TOPK_FREE(ptr) RedisModule_Free(ptr)
#else
#define TOPK_CALLOC(count, size) calloc(count, size)
#define TOPK_FREE(ptr) free(ptr)
#endif

#define TOPK_DECAY_LOOKUP_TABLE 256

typedef uint32_t counter_t;

typedef struct HeapBucket {
    uint32_t fp;
    uint32_t itemlen;
    char *item;
    counter_t count;
} HeapBucket;

typedef struct Bucket {
    uint32_t fp;        //  fingerprint
    counter_t count;
} Bucket;

typedef struct topk
{
    uint32_t k;
    uint32_t width;
    uint32_t depth;
    double decay;

    Bucket *data;
    struct HeapBucket *heap;
    double lookupTable[TOPK_DECAY_LOOKUP_TABLE];
    //  TODO: add function pointers for fast vs accurate
} TopK;


/*  Returns a new Top-K DS which will keep to 'k' heavyhitter, using 
    'depth' arrays of 'width' counters at 'decay' rate.
    Complexity - O(1) */
TopK *TopK_Create(uint32_t k, uint32_t width, uint32_t depth, double decay);

/*  Releases resources of a Top-K DS.
    Complexity - O(k) */
void TopK_Destroy(TopK *topk);

/*  Inserts an 'item' with length 'itemlen' into 'topk' DS. 
    Return value is NULL if no change to Top-K list occurred else,
    it returns the item expelled from list. If returned pointer 
    is not NULL, it should be free()d.
    Complexity - O(k) */
char *TopK_Add(TopK *topk, const char *item, size_t itemlen, uint32_t increment);

/*  Checks whether an 'item' is in Top-K list of 'topk'. 
    Complexity - O(k) */
bool TopK_Query(TopK *topk, const char *item, size_t itemlen);

/*  Returns count for an 'item' in 'topk' DS. 
    This number can be significantly lower than real count.
    Complexity - O(k) */
size_t TopK_Count(TopK *topk, const char *item, size_t itemlen);

/*  Returns full 'heapList' of items in 'topk' DS. */
void TopK_List(TopK *topk, char **heapList);

#endif
