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

    if (width > SIZE_MAX / depth || width * depth > SIZE_MAX / sizeof(uint32_t)) {
        return NULL;
    }

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
    if (!cms) {
        return;
    }

    if (cms->array) {
        CMS_FREE(cms->array);
        cms->array = NULL;
    }

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

static int checkOverflow(CMSketch *dest, size_t quantity, const CMSketch **src,
                         const long long *weights) {
    int64_t itemCount = 0;
    int64_t cmsCount = 0;
    size_t width = dest->width;
    size_t depth = dest->depth;

    for (size_t i = 0; i < depth; ++i) {
        for (size_t j = 0; j < width; ++j) {
            // Note: It is okay if itemCount becomes negative while looping.
            // e.g. weight[0] is negative. When the loop is done, total count
            // must be non-negative.
            itemCount = 0;
            for (size_t k = 0; k < quantity; ++k) {
                int64_t mul = 0;

                // Validation for:
                //   itemCount += src[k]->array[(i * width) + j] * weights[k];
                if (__builtin_mul_overflow(src[k]->array[(i * width) + j], weights[k], &mul) ||
                    (__builtin_add_overflow(itemCount, mul, &itemCount))) {
                    return -1;
                }
            }

            if (itemCount < 0 || itemCount > UINT32_MAX) {
                return -1;
            }
        }
    }

    for (size_t i = 0; i < quantity; ++i) {
        int64_t mul = 0;
        // Validation for
        //    cmsCount += src[i]->counter * weights[i];
        if (__builtin_mul_overflow(src[i]->counter, weights[i], &mul) ||
            (__builtin_add_overflow(cmsCount, mul, &cmsCount))) {
            return -1;
        }
    }

    if (cmsCount < 0) {
        return -1;
    }

    return 0;
}

int CMS_Merge(CMSketch *dest, size_t quantity, const CMSketch **src, const long long *weights) {
    assert(dest);
    assert(src);
    assert(weights);

    int64_t itemCount = 0;
    int64_t cmsCount = 0;
    size_t width = dest->width;
    size_t depth = dest->depth;

    if (checkOverflow(dest, quantity, src, weights) != 0) {
        return -1;
    }

    for (size_t i = 0; i < depth; ++i) {
        for (size_t j = 0; j < width; ++j) {
            itemCount = 0;
            for (size_t k = 0; k < quantity; ++k) {
                itemCount += (int64_t)src[k]->array[(i * width) + j] * weights[k];
            }
            dest->array[(i * width) + j] = itemCount;
        }
    }

    for (size_t i = 0; i < quantity; ++i) {
        cmsCount += src[i]->counter * weights[i];
    }
    dest->counter = cmsCount;

    return 0;
}

int CMS_MergeParams(mergeParams params) {
    return CMS_Merge(params.dest, params.numKeys, (const CMSketch **)params.cmsArray,
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
