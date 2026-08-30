/*
 * Copyright (c) 2006-Present, Redis Ltd.
 * All rights reserved.
 *
 * Licensed under your choice of (a) the Redis Source Available License 2.0
 * (RSALv2); or (b) the Server Side Public License v1 (SSPLv1); or (c) the
 * GNU Affero General Public License v3 (AGPLv3).
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

static inline uint64_t cellGet(const CMSketch *cms, size_t loc) {
    switch (cms->cellSize) {
    case 1:
        return ((const uint8_t *)cms->array)[loc];
    case 2:
        return ((const uint16_t *)cms->array)[loc];
    case 4:
        return ((const uint32_t *)cms->array)[loc];
    case 8:
        return ((const uint64_t *)cms->array)[loc];
    default:
        assert(0); // unreachable
        return 0;
    }
}

static inline void cellSet(CMSketch *cms, size_t loc, uint64_t value) {
    switch (cms->cellSize) {
    case 1:
        ((uint8_t *)cms->array)[loc] = (uint8_t)value;
        break;
    case 2:
        ((uint16_t *)cms->array)[loc] = (uint16_t)value;
        break;
    case 4:
        ((uint32_t *)cms->array)[loc] = (uint32_t)value;
        break;
    case 8:
        ((uint64_t *)cms->array)[loc] = value;
        break;
    default:
        assert(0); // unreachable
    }
}

CMSketch *NewCMSketch(size_t width, size_t depth, uint8_t cellSize) {
    assert(width > 0);
    assert(depth > 0);
    assert(CMS_IS_VALID_CELL_SIZE(cellSize));

    if (width > SIZE_MAX / depth || width * depth > SIZE_MAX / cellSize) {
        return NULL;
    }

    CMSketch *cms = CMS_CALLOC(1, sizeof(CMSketch));

    cms->width = width;
    cms->depth = depth;
    cms->counter = 0;
    cms->cellSize = cellSize;
    cms->array = CMS_TRYCALLOC(width * depth, cellSize);
    if (!cms->array) {
        CMS_FREE(cms);
        return NULL;
    }

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

CMSStatus CMS_IncrBy(CMSketch *cms, const char *item, size_t itemlen, int64_t value,
                     uint64_t *count) {
    assert(cms);
    assert(item);
    assert(count);

    const uint64_t cellMax = CMS_CELL_MAX(cms->cellSize);
    const uint64_t magnitude = (value < 0) ? -(uint64_t)value : (uint64_t)value;

    // First pass validates the whole operation, so that a rejected increment
    // leaves the sketch untouched.
    for (size_t i = 0; i < cms->depth; ++i) {
        uint32_t hash = CMS_HASH(item, itemlen, i);
        const uint64_t cell = cellGet(cms, (hash % cms->width) + (i * cms->width));
        if (value > 0 && magnitude > cellMax - cell) {
            return CMS_STATUS_OVERFLOW;
        }
        if (value < 0 && magnitude > cell) {
            return CMS_STATUS_UNDERFLOW;
        }
    }
    // The total count is replied as a RESP integer, so it is capped the same way
    // a cell is: a row holds `width` cells, so it could otherwise exceed INT64_MAX.
    if (value > 0 && magnitude > (uint64_t)INT64_MAX - cms->counter) {
        return CMS_STATUS_OVERFLOW;
    }
    // No sequence of commands can make counter smaller than a row's cells, but a
    // RESTORE payload can: the load path validates the dimensions and the buffer
    // length, never counter against the array. Without this, counter would wrap.
    if (value < 0 && magnitude > cms->counter) {
        return CMS_STATUS_UNDERFLOW;
    }

    uint64_t minCount = UINT64_MAX;
    for (size_t i = 0; i < cms->depth; ++i) {
        uint32_t hash = CMS_HASH(item, itemlen, i);
        size_t loc = (hash % cms->width) + (i * cms->width);
        const uint64_t updated =
            (value < 0) ? cellGet(cms, loc) - magnitude : cellGet(cms, loc) + magnitude;
        cellSet(cms, loc, updated);
        minCount = min(minCount, updated);
    }
    cms->counter = (value < 0) ? cms->counter - magnitude : cms->counter + magnitude;

    *count = minCount;
    return CMS_STATUS_OK;
}

uint64_t CMS_Query(CMSketch *cms, const char *item, size_t itemlen) {
    assert(cms);
    assert(item);

    uint64_t minCount = UINT64_MAX;

    for (size_t i = 0; i < cms->depth; ++i) {
        uint32_t hash = CMS_HASH(item, itemlen, i);
        minCount = min(minCount, cellGet(cms, (hash % cms->width) + (i * cms->width)));
    }
    return minCount;
}

int CMS_ValidateLoaded(const CMSketch *cms) {
    assert(cms);

    // The total count is replied as a RESP integer, and CMS_IncrBy computes its
    // headroom as INT64_MAX - counter, which wraps if counter is already above it.
    if (cms->counter > (uint64_t)INT64_MAX) {
        return -1;
    }

    // Only 8-byte cells can hold more than their maximum: the narrower widths
    // cannot physically represent a value above CMS_CELL_MAX.
    if (cms->cellSize == 8) {
        const uint64_t cellMax = CMS_CELL_MAX(cms->cellSize);
        for (size_t i = 0; i < cms->width * cms->depth; ++i) {
            if (cellGet(cms, i) > cellMax) {
                return -1;
            }
        }
    }

    return 0;
}

static int checkOverflow(CMSketch *dest, size_t quantity, const CMSketch **src,
                         const long long *weights) {
    int64_t itemCount = 0;
    int64_t cmsCount = 0;
    size_t width = dest->width;
    size_t depth = dest->depth;
    const int64_t cellMax = (int64_t)CMS_CELL_MAX(dest->cellSize);

    for (size_t i = 0; i < depth; ++i) {
        for (size_t j = 0; j < width; ++j) {
            // Note: It is okay if itemCount becomes negative while looping.
            // e.g. weight[0] is negative. When the loop is done, total count
            // must be non-negative.
            itemCount = 0;
            for (size_t k = 0; k < quantity; ++k) {
                int64_t mul = 0;

                // Validation for:
                //   itemCount += cellGet(src[k], (i * width) + j) * weights[k];
                if (__builtin_mul_overflow((int64_t)cellGet(src[k], (i * width) + j), weights[k],
                                           &mul) ||
                    (__builtin_add_overflow(itemCount, mul, &itemCount))) {
                    return -1;
                }
            }

            if (itemCount < 0 || itemCount > cellMax) {
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
                itemCount += (int64_t)cellGet(src[k], (i * width) + j) * weights[k];
            }
            cellSet(dest, (i * width) + j, (uint64_t)itemCount);
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
