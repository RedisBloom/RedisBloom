#include "cuckoo.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>

#ifndef CUCKOO_MALLOC
#define CUCKOO_MALLOC malloc
#define CUCKOO_CALLOC calloc
#define CUCKOO_REALLOC realloc
#define CUCKOO_FREE free
#endif

static int CuckooFilter_Grow(CuckooFilter *filter);

static size_t getNextN2(size_t n) {
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    n++;
    return n;
}

int CuckooFilter_Init(CuckooFilter *filter, size_t capacity) {
    memset(filter, 0, sizeof(*filter));
    filter->numBuckets = getNextN2(capacity / CUCKOO_BKTSIZE);
    if (filter->numBuckets == 0) {
        filter->numBuckets = 1; 
    }
    if (CuckooFilter_Grow(filter) != 0) {
        return -1;
    }
    return 0;
}

void CuckooFilter_Free(CuckooFilter *filter) {
    for (size_t ii = 0; ii < filter->numFilters; ++ii) {
        CUCKOO_FREE(filter->filters[ii]);
    }
    CUCKOO_FREE(filter->filters);
}

static int CuckooFilter_Grow(CuckooFilter *filter) {
    CuckooBucket **newFilter =
        CUCKOO_REALLOC(filter->filters, sizeof(*newFilter) * (filter->numFilters + 1));

    if (!newFilter) {
        return -1;
    }

    newFilter[filter->numFilters] = CUCKOO_CALLOC(filter->numBuckets, sizeof(CuckooBucket));
    if (!newFilter[filter->numFilters]) {
        return -1;
    }

    filter->numFilters++;
    filter->filters = newFilter;
    return 0;
}

typedef struct {
    size_t i1;
    size_t i2;
    CuckooFingerprint fp;
} LookupParams;

static size_t getAltIndex(CuckooFingerprint fp, size_t index, size_t numBuckets) {
    return ((uint32_t)(index ^ ((uint32_t)fp * 0x5bd1e995))) % numBuckets;
}

static void getLookupParams(CuckooHash hash, size_t numBuckets, LookupParams *params) {
    // Truncate the hash to uint8
    if ((params->fp = (hash >> 24)) == CUCKOO_NULLFP) {
        params->fp = 7;
    }

    params->i1 = hash % numBuckets;
    params->i2 = getAltIndex(params->fp, params->i1, numBuckets);
    // assert(getAltIndex(params->fp, params->i2, numBuckets) == params->i1);
}

static uint8_t *Bucket_Find(CuckooBucket bucket, size_t bucketSize, CuckooFingerprint fp) {
    for (size_t ii = 0; ii < bucketSize; ++ii) {
        if (bucket[ii] == fp) {
            return bucket + ii;
        }
    }
    return NULL;
}

static int Filter_Find(CuckooBucket *buckets, size_t bucketSize, const LookupParams *params) {
    return Bucket_Find(buckets[params->i1], bucketSize, params->fp) != NULL ||
           Bucket_Find(buckets[params->i2], bucketSize, params->fp) != NULL;
}

static int Bucket_Delete(CuckooBucket bucket, size_t bucketSize, CuckooFingerprint fp) {
    for (size_t ii = 0; ii < bucketSize; ii++) {
        if (bucket[ii] == fp) {
            bucket[ii] = CUCKOO_NULLFP;
            return 1;
        }
    }
    return 0;
}

static int Filter_Delete(CuckooBucket *buckets, size_t bucketSize, const LookupParams *params) {
    return Bucket_Delete(buckets[params->i1], bucketSize, params->fp) ||
           Bucket_Delete(buckets[params->i2], bucketSize, params->fp);
}

static int CuckooFilter_CheckFP(const CuckooFilter *filter, const LookupParams *params) {
    for (size_t ii = 0; ii < filter->numFilters; ++ii) {
        if (Filter_Find(filter->filters[ii], CUCKOO_BKTSIZE, params)) {
            return 1;
        }
    }
    return 0;
}

int CuckooFilter_Check(const CuckooFilter *filter, CuckooHash hash) {
    LookupParams params;
    getLookupParams(hash, filter->numBuckets, &params);
    return CuckooFilter_CheckFP(filter, &params);
}

static size_t bucketCount(const CuckooBucket bucket, size_t len, CuckooFingerprint fp) {
    size_t ret = 0;
    for (size_t ii = 0; ii < len; ++ii) {
        if (bucket[ii] == fp) {
            ret++;
        }
    }
    return ret;
}

static size_t filterCount(const CuckooBucket *filter, const LookupParams *params) {
    const size_t ixs[2] = {params->i1, params->i2};
    if (params->i1 == params->i2) {
        return bucketCount(filter[params->i1], CUCKOO_BKTSIZE, params->fp);
    }

    size_t ret = 0;
    for (size_t ii = 0; ii < 2; ++ii) {
        ret += bucketCount(filter[ixs[ii]], CUCKOO_BKTSIZE, params->fp);
    }
    return ret;
}

size_t CuckooFilter_Count(const CuckooFilter *filter, CuckooHash hash) {
    LookupParams params;
    getLookupParams(hash, filter->numBuckets, &params);
    size_t ret = 0;
    for (size_t ii = 0; ii < filter->numFilters; ++ii) {
        ret += filterCount((const CuckooBucket *)filter->filters[ii], &params);
    }
    return ret;
}

int CuckooFilter_Delete(CuckooFilter *filter, CuckooHash hash) {
    LookupParams params;
    getLookupParams(hash, filter->numBuckets, &params);
    for (size_t ii = 0; ii < filter->numFilters; ++ii) {
        if (Filter_Delete(filter->filters[ii], CUCKOO_BKTSIZE, &params)) {
            filter->numItems--;
            filter->numDeletes++;
            if (filter->numFilters > 1 && filter->numDeletes > (double)filter->numItems * 0.10) {
                CuckooFilter_Compact(filter);
            }
            return 1;
        }
    }
    return 0;
}

static uint8_t *Bucket_FindAvailable(CuckooBucket bucket, size_t bucketSize) {
    for (size_t ii = 0; ii < bucketSize; ++ii) {
        if (bucket[ii] == CUCKOO_NULLFP) {
            return &bucket[ii];
        }
    }
    return NULL;
}

static uint8_t *Filter_FindAvailable(CuckooBucket *filter, size_t bucketSize,
                                     const LookupParams *params) {
    uint8_t *slot;
    if ((slot = Bucket_FindAvailable(filter[params->i1], CUCKOO_BKTSIZE)) ||
        (slot = Bucket_FindAvailable(filter[params->i2], CUCKOO_BKTSIZE))) {
        return slot;
    }
    return NULL;
}

static uint8_t *Filter_FindAvailableDbg(CuckooBucket *filter, size_t bucketSize,
                                        const LookupParams *params, size_t *newIx) {
    uint8_t *slot;
    if ((slot = Bucket_FindAvailable(filter[params->i1], CUCKOO_BKTSIZE))) {
        *newIx = params->i1;
    } else if ((slot = Bucket_FindAvailable(filter[params->i2], CUCKOO_BKTSIZE))) {
        *newIx = params->i2;
    } else {
        *newIx = -1;
    }
    return slot;
}

static uint8_t *Filter_FindUnique(CuckooBucket bucket, size_t index, size_t bucketSize,
                                  CuckooFingerprint fp, CuckooInsertStatus *err) {
    uint8_t *firstEmpty = NULL;
    bucket += (index * bucketSize);
    for (size_t ii = 0; ii < bucketSize; ++ii) {
        if (bucket[ii] == fp) {
            *err = CuckooInsert_Exists;
            return NULL;
        } else if (firstEmpty == 0 && bucket[ii] == 0) {
            firstEmpty = bucket + ii;
        }
    }
    if (firstEmpty == NULL) {
        *err = CuckooInsert_NoSpace;
    }
    return firstEmpty;
}

#define MAX_ITERATIONS 500
static CuckooInsertStatus Filter_KOInsert(CuckooBucket *curFilter, size_t numBuckets,
                                          size_t bucketSize, const LookupParams *params,
                                          LookupParams *victim);

static CuckooInsertStatus CuckooFilter_InsertFP(CuckooFilter *filter, const LookupParams *params) {
    CuckooBucket *curFilter = filter->filters[filter->numFilters - 1];
    uint8_t *slot = Filter_FindAvailable(curFilter, CUCKOO_BKTSIZE, params);
    if (slot) {
        *slot = params->fp;
        filter->numItems++;
        return CuckooInsert_Inserted;
    }

    // No space. Time to evict!
    LookupParams victim = {0};
    CuckooInsertStatus status =
        Filter_KOInsert(curFilter, filter->numBuckets, CUCKOO_BKTSIZE, params, &victim);
    if (status == CuckooInsert_Inserted) {
        filter->numItems++;
        return status;
    }

    if (CuckooFilter_Grow(filter) != 0) {
        return CuckooInsert_NoSpace;
    }

    // Try to insert the filter again
    return CuckooFilter_InsertFP(filter, &victim);
}

CuckooInsertStatus CuckooFilter_Insert(CuckooFilter *filter, CuckooHash hash) {
    LookupParams params;
    getLookupParams(hash, filter->numBuckets, &params);
    return CuckooFilter_InsertFP(filter, &params);
}

CuckooInsertStatus CuckooFilter_InsertUnique(CuckooFilter *filter, CuckooHash hash) {
    LookupParams params;
    getLookupParams(hash, filter->numBuckets, &params);
    if (CuckooFilter_CheckFP(filter, &params)) {
        return CuckooInsert_Exists;
    }
    return CuckooFilter_InsertFP(filter, &params);
}

static CuckooInsertStatus Filter_KOInsert(CuckooBucket *curFilter, size_t numBuckets,
                                          size_t bucketSize, const LookupParams *params,
                                          LookupParams *victim) {
    // printf("Starting kickout sequence.. FP: %d, I1=%lu, I2=%lu\n", params->fp, params->i1,
    //        params->i2);
    CuckooFingerprint fp = params->fp;
    size_t counter = 0;
    size_t ii = params->i1;
    // params = NULL; // Don't reference 'params' again!

    while (counter++ < MAX_ITERATIONS) {
        uint8_t *bucket = curFilter[ii];

        // Try random record to evict
        size_t victimIx = rand() % bucketSize;
        CuckooFingerprint tmpFp = bucket[victimIx];
        // printf("Cur: %d. Victim: %d\n", fp, tmpFp);
        bucket[victimIx] = fp;
        fp = tmpFp;
        ii = getAltIndex(fp, ii, numBuckets);

        // Insert the new item in potentially the same bucket
        uint8_t *empty = Bucket_FindAvailable(curFilter[ii], bucketSize);
        if (empty) {
            // printf("Found slot. Bucket[%lu], Pos=%lu\n", ii, empty - curFilter[ii]);
            // printf("Old FP Value: %d\n", *empty);
            // printf("Setting FP: %p\n", empty);
            *empty = fp;
            return CuckooInsert_Inserted;
        }
    }

    victim->fp = fp;
    victim->i1 = ii;
    victim->i2 = getAltIndex(victim->fp, victim->i1, numBuckets);
    return CuckooInsert_NoSpace;
}

#define RELOC_EMPTY 0
#define RELOC_OK 1
#define RELOC_FAIL -1

/**
 * Attempt to move a slot from one bucket to another filter
 */
static int relocateSlot(CuckooFilter *cf, CuckooBucket bucket, size_t filterIx, size_t bucketIx,
                        size_t slotIx) {
    LookupParams params = { 0 };
    if ((params.fp = bucket[slotIx]) == CUCKOO_NULLFP) {
        // Nothing in this slot.
        return RELOC_EMPTY;
    }

    params.i1 = bucketIx;
    params.i2 = getAltIndex(params.fp, bucketIx, cf->numBuckets);

    // Look at all the prior filters and attempt to find a home
    for (size_t ii = 0; ii < filterIx; ++ii) {
        size_t foundIx;
        uint8_t *slot = Filter_FindAvailableDbg(cf->filters[ii], CUCKOO_BKTSIZE, &params, &foundIx);
        if (slot) {
            *slot = params.fp;
            bucket[slotIx] = CUCKOO_NULLFP;
            return RELOC_OK;
        }
    }
    return RELOC_FAIL;
}

/**
 * Attempt to strip a single filter moving it down a slot
 */
static size_t CuckooFilter_CompactSingle(CuckooFilter *cf, size_t filterIx) {
    CuckooBucket *filter = cf->filters[filterIx];
    int dirty = 0;
    size_t numRelocs = 0;

    for (size_t bucketIx = 0; bucketIx < cf->numBuckets; ++bucketIx) {
        for (size_t slotIx = 0; slotIx < CUCKOO_BKTSIZE; ++slotIx) {
            int status = relocateSlot(cf, filter[bucketIx], filterIx, bucketIx, slotIx);
            if (status == RELOC_FAIL) {
                dirty = 1;
            } else if (status == RELOC_OK) {
                numRelocs++;
            }
        }
    }
    if (!dirty) {
        CUCKOO_FREE(filter);
        cf->numFilters--;
    }
    return numRelocs;
}

size_t CuckooFilter_Compact(CuckooFilter *cf) {
    size_t ret = 0;
    for (size_t ii = cf->numFilters; ii > 1; --ii) {
        ret += CuckooFilter_CompactSingle(cf, ii - 1);
    }
    cf->numDeletes = 0;
    return ret;
}

/* CF.DEBUG uses another function
void CuckooFilter_GetInfo(const CuckooFilter *cf, CuckooHash hash, CuckooKey *out) {
    LookupParams params;
    getLookupParams(hash, cf->numBuckets, &params);
    out->fp = params.fp;
    out->i1 = params.i1;
    out->i2 = params.i2;
    assert(getAltIndex(params.fp, out->i1, cf->numBuckets) == out->i2);
    assert(getAltIndex(params.fp, out->i2, cf->numBuckets) == out->i1);
}*/