/*
 * Copyright Redis Ltd. 2019 - present
 * Licensed under your choice of the Redis Source Available License 2.0 (RSALv2) or
 * the Server Side Public License v1 (SSPLv1).
 */

#include "cms.h"
#include "murmur2/murmurhash2.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define min(a, b) (((a) < (b)) ? (a) : (b))

#define BIT64 64
#define CMS_HASH(item, itemlen, i) MurmurHash2(item, itemlen, i)

CMSketch *NewCMSketch(size_t width, size_t depth) {
    assert(width > 0);
    assert(depth > 0);

    CMSketch *cms = CMS_CALLOC(1, sizeof(CMSketch));

    cms->width = width;
    cms->depth = depth;
    cms->counter = 0;
    cms->array = CMS_CALLOC(width * depth, sizeof(uint32_t));

    return cms;
}

void CMS_DimFromProb(double error, double delta, size_t *width, size_t *depth) {
    assert(error > 0 && error < 1);
    assert(delta > 0 && delta < 1);

    *width = ceil(2 / error);
    *depth = ceil(log10f(delta) / log10f(0.5));
}

void CMS_Destroy(CMSketch *cms) {
    assert(cms);

    CMS_FREE(cms->array);
    cms->array = NULL;

    CMS_FREE(cms);
}

size_t CMS_IncrBy(CMSketch *cms, const char *item, size_t itemlen, size_t value) {
    assert(cms);
    assert(item);

    size_t minCount = (size_t)-1;

    for (size_t i = 0; i < cms->depth; ++i) {
        uint32_t hash = CMS_HASH(item, itemlen, i);
        size_t loc = (hash % cms->width) + (i * cms->width);
        cms->array[loc] += value;
        if (cms->array[loc] < value) {
            cms->array[loc] = UINT32_MAX;
        }
        minCount = min(minCount, cms->array[loc]);
    }
    cms->counter += value;
    return minCount;
}

size_t CMS_Query(CMSketch *cms, const char *item, size_t itemlen) {
    assert(cms);
    assert(item);

    size_t minCount = (size_t)-1;

    for (size_t i = 0; i < cms->depth; ++i) {
        uint32_t hash = CMS_HASH(item, itemlen, i);
        minCount = min(minCount, cms->array[(hash % cms->width) + (i * cms->width)]);
    }
    return minCount;
}

void CMS_Merge(CMSketch *dest, size_t quantity, const CMSketch **src, const long long *weights) {
    assert(dest);
    assert(src);
    assert(weights);

    size_t itemCount = 0;
    size_t cmsCount = 0;
    size_t width = dest->width;
    size_t depth = dest->depth;

    for (size_t i = 0; i < depth; ++i) {
        for (size_t j = 0; j < width; ++j) {
            itemCount = 0;
            for (size_t k = 0; k < quantity; ++k) {
                itemCount += src[k]->array[(i * width) + j] * weights[k];
            }
            dest->array[(i * width) + j] = itemCount;
        }
    }

    for (size_t i = 0; i < quantity; ++i) {
        cmsCount += src[i]->counter * weights[i];
    }
    dest->counter = cmsCount;
}

void CMS_MergeParams(mergeParams params) {
    CMS_Merge(params.dest, params.numKeys, (const CMSketch **)params.cmsArray,
              (const long long *)params.weights);
}

/************ used for debugging *******************
void CMS_Print(const CMSketch *cms) {
    assert(cms);

    for (int i = 0; i < cms->depth; ++i) {
        for (int j = 0; j < cms->width; ++j) {
            printf("%d\t", cms->array[(i * cms->width) + j]);
        }
        printf("\n");
    }
    printf("\tCounter is %lu\n", cms->counter);
} */