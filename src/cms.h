#ifndef RM_CMS_H
#define RM_CMS_H

#include <stddef.h>

// #define REDIS_MODULE_TARGET

#ifdef REDIS_MODULE_TARGET // should be in .h or .c
#define CMS_CALLOC(count, size) RedisModule_Calloc(count, size)
#define CMS_FREE(ptr) RedisModule_Free(ptr)
#else
#define CMS_CALLOC(count, size) calloc(count, size)
#define CMS_FREE(ptr) free(ptr)
#endif

typedef struct
{
    size_t width;
    size_t depth;
    size_t *array;    
    size_t counter; // might be used for top k results
} CMSketch;

CMSketch *NewCMSketch(size_t width, size_t depth);
void CMS_Destroy(CMSketch *cms);
void CMS_IncrBy(CMSketch *cms, const char *item, size_t strlen, size_t value);
size_t CMS_Query(CMSketch *cms, const char *item, size_t strlen);
void CMS_Merge(CMSketch *dest, size_t quantity, 
               const CMSketch **src, const long long *weights);
void CMS_Print(const CMSketch *cms);

#endif