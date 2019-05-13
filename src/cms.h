#ifndef RM_CMS_H
#define RM_CMS_H

#include <stdint.h> // uint32_t

// #define REDIS_MODULE_TARGET

#ifdef REDIS_MODULE_TARGET // should be in .h or .c
#define CMS_CALLOC(count, size) RedisModule_Calloc(count, size)
#define CMS_FREE(ptr) RedisModule_Free(ptr)
#else
#define CMS_CALLOC(count, size) calloc(count, size)
#define CMS_FREE(ptr) free(ptr)
#endif

typedef struct CMS {
    size_t width;
    size_t depth;
    uint32_t *array;
    size_t counter;
    // TODO: requested n + current cardinality
} CMSketch;

/* Creates a new Count-Min Sketch with dimentions of width * depth */
CMSketch *NewCMSketch(size_t width, size_t depth);

/*  Recommands width & depth for expected n different items,
    with probability of an error  - prob and over estimation
    error - overEst (use 1 for max accuracy) */
void CMS_DimFromProb(size_t n, double overEst, double prob, size_t *width, size_t *depth);

void CMS_Destroy(CMSketch *cms);

/*  Increases item count in value.
    Value must be a non negative number */
void CMS_IncrBy(CMSketch *cms, const char *item, size_t strlen, size_t value);

/* Returns an estimate counter for item */
size_t CMS_Query(CMSketch *cms, const char *item, size_t strlen);

/*  Merges multiple CMSketches into a single one.
    All sketches must have identical width and depth.
    dest must be already initilized.
*/
void CMS_Merge(CMSketch *dest, size_t quantity, const CMSketch **src, const long long *weights);

/* Help function */
void CMS_Print(const CMSketch *cms);

/* Checks cardinality of sketch */
size_t CMS_GetCardinality(CMSketch *cms);

#endif