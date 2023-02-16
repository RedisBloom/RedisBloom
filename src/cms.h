/*
 * Copyright Redis Ltd. 2019 - present
 * Licensed under your choice of the Redis Source Available License 2.0 (RSALv2) or
 * the Server Side Public License v1 (SSPLv1).
 */

#pragma once

#include <stdint.h> // uint32_t

#ifdef REDIS_MODULE_TARGET
#include "redismodule.h"
#define CMS_CALLOC(count, size) RedisModule_Calloc(count, size)
#define CMS_FREE(ptr) RedisModule_Free(ptr)
#else
//#define CMS_CALLOC(count, size) calloc(count, size)
//#define CMS_FREE(ptr) free(ptr)
#endif

typedef struct CMS {
    size_t width;
    size_t depth;
    uint32_t *array;
    size_t counter;
} CMSketch;

typedef struct {
    CMSketch *dest;
    long long numKeys;
    CMSketch **cmsArray;
    long long *weights;
} mergeParams;

/* Creates a new Count-Min Sketch with dimensions of width * depth */
CMSketch *NewCMSketch(size_t width, size_t depth);

/*  Recommends width & depth for expected n different items,
    with probability of an error  - prob and over estimation
    error - overEst (use 1 for max accuracy) */
void CMS_DimFromProb(double overEst, double prob, size_t *width, size_t *depth);

void CMS_Destroy(CMSketch *cms);

/*  Increases item count in value.
    Value must be a non negative number */
size_t CMS_IncrBy(CMSketch *cms, const char *item, size_t strlen, size_t value);

/* Returns an estimate counter for item */
size_t CMS_Query(CMSketch *cms, const char *item, size_t strlen);

/*  Merges multiple CMSketches into a single one.
    All sketches must have identical width and depth.
    dest must be already initialized.
*/
void CMS_Merge(CMSketch *dest, size_t quantity, const CMSketch **src, const long long *weights);
void CMS_MergeParams(mergeParams params);

/* Help function */
void CMS_Print(const CMSketch *cms);
