#ifndef CF_H
#define CF_H
#include "cuckoo.h"

const char *CF_GetEncodedChunk(const CuckooFilter *cf, long long *pos, size_t *buflen,
                               size_t bytelimit);
int CF_LoadEncodedChunk(const CuckooFilter *cf, long long pos, const char *data, size_t datalen);

typedef struct __attribute__((packed)) {
    uint64_t numItems;
    uint64_t numBuckets;
    uint64_t numDeletes;
    uint64_t numFilters;
    uint16_t bucketSize;
    uint16_t maxIterations;
    uint16_t expansion;
    uint32_t *filtersNumBucket;
} CFHeader;

CuckooFilter *CFHeader_Load(const CFHeader *header);
void fillCFHeader(CFHeader *header, const CuckooFilter *cf);

#endif