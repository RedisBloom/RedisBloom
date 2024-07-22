/*
 * Copyright Redis Ltd. 2017 - present
 * Licensed under your choice of the Redis Source Available License 2.0 (RSALv2) or
 * the Server Side Public License v1 (SSPLv1).
 */

#pragma once

#include "murmur2/murmurhash2.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

// Defines whether 32bit or 64bit hash function will be used.
// It will be deprecated in RedisBloom 3.0.

#define CUCKOO_BKTSIZE 2
#define CUCKOO_NULLFP 0
// extern int globalCuckooHash64Bit;

typedef uint8_t CuckooFingerprint;
typedef uint64_t CuckooHash;
typedef uint8_t CuckooBucket[1];
typedef uint8_t MyCuckooBucket;

#define CF_DEFAULT_MAX_ITERATIONS 20
#define CF_DEFAULT_BUCKETSIZE 2
#define CF_DEFAULT_EXPANSION_FACTOR 1
#define CF_MAX_EXPANSION_FACTOR 32768
#define CF_MAX_ITERATIONS 65535
#define CF_MAX_BUCKET_SIZE 255                     // 8 bits, see struct SubCF
#define CF_MAX_NUM_BUCKETS (0x00FFFFFFFFFFFFFFULL) // 56 bits, see struct SubCF
#define CF_MAX_NUM_FILTERS (UINT16_MAX)            // 16 bits, see struct CuckooFilter

typedef struct {
    uint64_t numBuckets : 56;
    uint64_t bucketSize : 8;
    MyCuckooBucket *data;
} SubCF;

typedef struct {
    uint64_t numBuckets;
    uint64_t numItems;
    uint64_t numDeletes;
    uint16_t numFilters;
    uint16_t bucketSize;
    uint16_t maxIterations;
    uint16_t expansion;
    SubCF *filters;
} CuckooFilter;

#define CUCKOO_GEN_HASH(s, n) MurmurHash64A_Bloom(s, n, 0)

/*
#define CUCKOO_GEN_HASH(s, n)                       \
            globalCuckooHash64Bit == 1 ?            \
                MurmurHash64A_Bloom(s, n, 0) :      \
                murmurhash2(s, n, 0)
*/
typedef struct {
    uint64_t i1;
    uint64_t i2;
    CuckooFingerprint fp;
} CuckooKey;

typedef enum {
    CuckooInsert_Inserted = 1,
    CuckooInsert_Exists = 0,
    CuckooInsert_NoSpace = -1,
    CuckooInsert_MemAllocFailed = -2
} CuckooInsertStatus;

int CuckooFilter_Init(CuckooFilter *filter, uint64_t capacity, uint16_t bucketSize,
                      uint16_t maxIterations, uint16_t expansion);
void CuckooFilter_Free(CuckooFilter *filter);
CuckooInsertStatus CuckooFilter_InsertUnique(CuckooFilter *filter, CuckooHash hash);
CuckooInsertStatus CuckooFilter_Insert(CuckooFilter *filter, CuckooHash hash);
int CuckooFilter_Delete(CuckooFilter *filter, CuckooHash hash);
int CuckooFilter_Check(const CuckooFilter *filter, CuckooHash hash);
uint64_t CuckooFilter_Count(const CuckooFilter *filter, CuckooHash);
void CuckooFilter_Compact(CuckooFilter *filter, bool cont);
void CuckooFilter_GetInfo(const CuckooFilter *cf, CuckooHash hash, CuckooKey *out);
int CuckooFilter_ValidateIntegrity(const CuckooFilter *cf);
