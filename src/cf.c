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
    *offset = pos % cf->numBuckets;

    // Get the actual filter index
    size_t filterIx = (pos - *offset) / cf->numFilters;

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
    return cf->filters[filterIx][*offset];
}

const char *CF_GetEncodedChunk(const CuckooFilter *cf, long long *pos, size_t *buflen,
                               size_t bytelimit) {
    size_t offset;
    uint8_t *bucket = getBucketPos(cf, *pos, &offset);
    if (!bucket) {
        return NULL;
    }
    size_t chunksz = cf->numBuckets - offset;
    size_t max_buckets = (bytelimit / CUCKOO_BKTSIZE);
    if (chunksz > max_buckets) {
        chunksz = max_buckets;
    }
    *pos += chunksz;
    *buflen = chunksz * CUCKOO_BKTSIZE;
    return (const char *)bucket;
}

int CF_LoadEncodedChunk(const CuckooFilter *cf, long long pos, const char *data, size_t datalen) {
    if (datalen == 0 || datalen % CUCKOO_BKTSIZE != 0) {
        return REDISMODULE_ERR;
    }
    size_t nbuckets = datalen / CUCKOO_BKTSIZE;
    if (nbuckets > pos) {
        return REDISMODULE_ERR;
    }
    pos -= nbuckets;

    size_t offset;
    uint8_t *bucketpos = getBucketPos(cf, pos, &offset);
    if (bucketpos == NULL) {
        return REDISMODULE_ERR;
    }
    if (offset + nbuckets > cf->numBuckets) {
        return REDISMODULE_ERR;
    }
    memcpy(bucketpos, data, datalen);
    return REDISMODULE_OK;
}

CuckooFilter *CFHeader_Load(const CFHeader *header) {
    CuckooFilter *filter = RedisModule_Calloc(1, sizeof(*filter));
    filter->numBuckets = header->numBuckets;
    filter->numFilters = header->numFilters;
    filter->numItems = header->numItems;
    filter->numDeletes = header->numDeletes;
    filter->filters = RedisModule_Alloc(sizeof(*filter->filters) * header->numFilters);
    for (size_t ii = 0; ii < filter->numFilters; ++ii) {
        filter->filters[ii] = RedisModule_Calloc(filter->numBuckets, sizeof(CuckooBucket));
    }
    return filter;
}