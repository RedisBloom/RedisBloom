#include "redismodule.h"
#define CUCKOO_MALLOC RedisModule_Alloc
#define CUCKOO_CALLOC RedisModule_Calloc
#define CUCKOO_REALLOC RedisModule_Realloc
#define CUCKOO_FREE RedisModule_Free
#include "cuckoo.c"
#include "cf.h"

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
    if (datalen == 0) {
        return REDISMODULE_ERR;
    }

    // calculate offset
    long long offset = pos - datalen - 1;
    long long currentSize;
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

    // copy data to filter
    memcpy(filter->data + offset, data, datalen);
    return REDISMODULE_OK;
}

CuckooFilter *CFHeader_Load(const CFHeader *header) {
    CuckooFilter *filter = RedisModule_Calloc(1, sizeof(*filter));
    filter->numBuckets = header->numBuckets;
    filter->numFilters = header->numFilters;
    filter->numItems = header->numItems;
    filter->numDeletes = header->numDeletes;
    filter->bucketSize = header->bucketSize;
    filter->maxIterations = header->maxIterations;
    filter->expansion = header->expansion;
    filter->filters = RedisModule_Alloc(sizeof(*filter->filters) * header->numFilters);
    for (size_t ii = 0; ii < filter->numFilters; ++ii) {
        SubCF *cur = filter->filters + ii;
        cur->bucketSize = header->bucketSize;
        cur->numBuckets = header->filtersNumBucket[ii];
        cur->data =
            RedisModule_Calloc((size_t)cur->numBuckets * filter->bucketSize, sizeof(CuckooBucket));
    }
    RedisModule_Free(header->filtersNumBucket);
    return filter;
}

void fillCFHeader(CFHeader *header, const CuckooFilter *cf) {
    *header = (CFHeader){.numItems = cf->numItems,
                         .numBuckets = cf->numBuckets,
                         .numDeletes = cf->numDeletes,
                         .numFilters = cf->numFilters,
                         .bucketSize = cf->bucketSize,
                         .maxIterations = cf->maxIterations,
                         .expansion = cf->expansion};
    header->filtersNumBucket =
        RedisModule_Calloc(cf->numFilters, sizeof(*header->filtersNumBucket));
    for (size_t ii = 0; ii < header->numFilters; ++ii) {
        header->filtersNumBucket[ii] = cf->filters[ii].numBuckets;
    }
}