#include <stdlib.h>         // malloc
#include <string.h>         // strlen
#include <stdio.h>          // printf
#include <math.h>           // q, ceil
#include <assert.h>         // assert

#include "cms.h"

CMSketch *NewCMSketch(size_t width, size_t depth) {
    assert(width > 0);
    assert(depth > 0);

    CMSketch *cms = NULL;
    if((cms = (CMSketch *)CMS_CALLOC(1, sizeof(CMSketch))) == NULL) {
        return NULL;
    }

    cms->width = width;
    cms->depth = depth;
    cms->counter = 0;
    cms->array = (size_t *)CMS_CALLOC(width * depth, sizeof(size_t));
    if(cms->array == NULL) {
        CMS_FREE(cms);
        return NULL;
    } 

    return cms; 
}

void CMS_Destroy(CMSketch *cms) {
    assert(cms);

    CMS_FREE(cms->array);
    cms->array = NULL;

    CMS_FREE(cms);
}

void CMS_IncrBy(CMSketch *cms, const char *item, size_t value) {
    assert(cms);
    assert(item);

    for(size_t i = 0; i < cms->depth; ++i) {
        size_t hash = XXH64(item, strlen(item), i);
        cms->array[(hash % cms->width) + (i * cms->width)] += value;
    }
    cms->counter += value;
}

size_t CMS_Query(CMSketch *cms, const char *item) {
    assert(cms);
    assert(item);

    size_t temp = 0, res = ~0;

    for(size_t i = 0; i < cms->depth; ++i) {
        size_t hash = XXH64(item, strlen(item), i);
        temp = cms->array[(hash % cms->width) + (i * cms->width)];
        if(temp < res) res = temp;
    }

    return res;
}

void CMS_Merge(CMSketch *dest, size_t quantity, CMSketch **src, long long *weight) {
    assert(dest);
    assert(src);
    assert(weight);

    size_t tempCount= 0;
    long128 tempTotal = 0;
    size_t width = dest->width;
    size_t depth = dest->depth;
    
    for(size_t i = 0; i < depth; ++i) {
        for(size_t j = 0; j < width; ++j) {            
            tempCount = 0;
            for(size_t k = 0; k < quantity; ++k) {
                tempCount += src[k]->array[(i * width) + j] * weight[k];
            }
            dest->array[(i * width) + j] = tempCount; 
        }
    }

    for(size_t i = 0; i < quantity; ++i) {
        tempTotal += src[i]->counter * weight[i];
    }
    dest->counter = tempTotal;
}
 
void CMS_Print(const CMSketch *cms) {
    assert(cms);

    for(int i = 0; i < cms->depth; ++i) {
        for(int j = 0; j < cms->width; ++j) {
            printf("%lu\t", cms->array[(i * cms->width) + j]);
        }
        printf("\n");
    }
    printf("\tCounter is %lu\n", (size_t)cms->counter);
}