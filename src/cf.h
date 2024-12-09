/*
 * Copyright Redis Ltd. 2017 - present
 * Licensed under your choice of the Redis Source Available License 2.0 (RSALv2) or
 * the Server Side Public License v1 (SSPLv1).
 */

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
} CFHeader;

CuckooFilter *CFHeader_Load(const CFHeader *header);
CFHeader fillCFHeader(const CuckooFilter *cf);

#endif