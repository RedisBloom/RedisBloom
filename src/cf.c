/*
 * Copyright (c) 2006-Present, Redis Ltd.
 * All rights reserved.
 *
 * Licensed under your choice of (a) the Redis Source Available License 2.0
 * (RSALv2); or (b) the Server Side Public License v1 (SSPLv1); or (c) the
 * GNU Affero General Public License v3 (AGPLv3).
 */

#include "redismodule.h"

#define CUCKOO_MALLOC RedisModule_Alloc
#define CUCKOO_CALLOC RedisModule_Calloc
#define CUCKOO_TRYCALLOC(...)                                                                      \
    RedisModule_TryCalloc ? RedisModule_TryCalloc(__VA_ARGS__) : RedisModule_Calloc(__VA_ARGS__)
#define CUCKOO_REALLOC RedisModule_Realloc
#define CUCKOO_FREE RedisModule_Free
#include "cuckoo.c"
#include "cf.h"

#if 0 // unused

// Get the bucket corresponding to the given position. 'offset' is modified to be the
// actual position (beginning of bucket) where `pos` is mapped to, with respect to
// the current filter. The filter itself is not returned directly (but can be inferred)
// via `offset`
static uint8_t *getBucketPos(const CuckooFilter *cf, long long pos, size_t *offset) {
    // Normalize the pos pointer to the beginning of the filter
    pos--;
    *offset = pos % cf->numBuckets;
    // Get the actual filter index.
    size_t filterIx = (pos - (pos % cf->numBuckets)) / cf->numBuckets;

    if (filterIx >= cf->numFilters) {
        // Last position
        return NULL;
    }

    if (*offset + 1 == cf->numBuckets) {
        *offset = 0;
        if (++filterIx == cf->numFilters) {
            return NULL;
        }
    }
    return cf->filters[filterIx].data + *offset;
}

#endif // 0

const char *CF_GetEncodedChunk(const CuckooFilter *cf, long long *pos, size_t *buflen,
                               size_t bytelimit) {
    // First find filter
    long long offset = *pos - 1;
    long long currentSize = 0;
    int filterIx = 0;
    SubCF *filter;
    for (; filterIx < cf->numFilters; ++filterIx) {
        filter = cf->filters + filterIx;
        currentSize = filter->bucketSize * filter->numBuckets;
        if (offset < currentSize) {
            break;
        }
        offset -= currentSize;
    }

    // all filters already returned
    if (filterIx == cf->numFilters) {
        return NULL;
    }

    if (currentSize <= bytelimit) {
        // filter size is smaller than the limit. Add it all
        *buflen = currentSize;
        *pos += currentSize;
        return (const char *)filter->data;
    } else {
        size_t remaining = currentSize - offset;
        if (remaining > bytelimit) {
            // return another piece from the filter
            *buflen = bytelimit;
            *pos += bytelimit;
        } else {
            // return the rest of the filter
            *buflen = remaining;
            *pos += remaining;
        }
        return (const char *)filter->data + offset;
    }
}

int CF_LoadEncodedChunk(const CuckooFilter *cf, long long pos, const char *data, size_t datalen) {
    if (datalen == 0 || pos <= 0 || (size_t)(pos - 1) < datalen) {
        return REDISMODULE_ERR;
    }

    // calculate offset
    long long offset = pos - datalen - 1;
    long long currentSize;
    int filterIx = 0;
    SubCF *filter = NULL;
    for (; filterIx < cf->numFilters; ++filterIx) {
        filter = cf->filters + filterIx;
        currentSize = filter->bucketSize * filter->numBuckets;
        if (offset < currentSize) {
            break;
        }
        offset -= currentSize;
    }

    // Boundary check before memcpy()
    if (!filter || ((size_t)offset > SIZE_MAX - datalen) ||
        filter->bucketSize * filter->numBuckets < offset + datalen) {
        return REDISMODULE_ERR;
    }

    // copy data to filter
    memcpy(filter->data + offset, data, datalen);
    return REDISMODULE_OK;
}

CuckooFilter *CFHeader_Load(const CFHeader *header) {
    CuckooFilter *filter = RedisModule_Calloc(1, sizeof *filter);
    filter->numBuckets = header->numBuckets;
    filter->numFilters = header->numFilters;
    filter->numItems = header->numItems;
    filter->numDeletes = header->numDeletes;
    filter->bucketSize = header->bucketSize;
    filter->maxIterations = header->maxIterations;
    filter->expansion = header->expansion;
    filter->filters = RedisModule_Calloc(filter->numFilters, sizeof *filter->filters);

    if (CuckooFilter_ValidateIntegrity(filter) != 0) {
        goto error;
    }

    for (size_t ii = 0; ii < filter->numFilters; ++ii) {
        SubCF *cur = filter->filters + ii;
        cur->data = NULL;
        cur->bucketSize = header->bucketSize;
        cur->numBuckets = filter->numBuckets * pow(filter->expansion, ii);

        if (filter->bucketSize > SIZE_MAX / cur->numBuckets) {
            goto error;
        }

        cur->data =
            CUCKOO_TRYCALLOC((size_t)cur->numBuckets * filter->bucketSize, sizeof(CuckooBucket));
        if (!cur->data) {
            goto error;
        }
    }
    return filter;

error:
    CuckooFilter_Free(filter);
    RedisModule_Free(filter);
    return NULL;
}

CFHeader fillCFHeader(const CuckooFilter *cf) {
    return (CFHeader){
        .numItems = cf->numItems,
        .numBuckets = cf->numBuckets,
        .numDeletes = cf->numDeletes,
        .numFilters = cf->numFilters,
        .bucketSize = cf->bucketSize,
        .maxIterations = cf->maxIterations,
        .expansion = cf->expansion,
    };
}
