/*
 * Copyright 2019-2020 Redis Labs Ltd. and Contributors
 *
 * This file is available under the Redis Labs Source Available License Agreement
 *
 * The Bloom-Filter variant is based on the paper "Age-Partitioned Bloom Filters"
 * by Paulo Sérgio Almeida, Carlos Baquero and Ariel Shtul
 * Link: https://arxiv.org/pdf/2001.03147.pdf
 */

/*
 *  Age-Partitioned Bloom-Filer is an implementation of Bloom Filter.
 *  In APBF there are hash + timeframes arrays which are independent of
 *  one another. This allows for :
 *
 *    * Querying of a subset of arrays, limiting the timeframe in which the
 *      item was inserted.
 *
 *    * Erasing a whole array when expired which saves space.
 *
 *  Note : Expired items have a higher false positive until `timeframes` number
 *  of slot shifts occurred.
 */

#ifndef __H_FLEX_BLOOM__
#define __H_FLEX_BLOOM__

#include <time.h>
#include <stdint.h>
#include <stdbool.h>

#define CHECK_ALL 0

#define TWOHASH

typedef struct ageBloom_s ageBloom_t;
typedef uint64_t timestamp_t;
typedef ageBloom_t ageBloom; // back support

typedef struct {
    uint64_t size;
    uint64_t count;
    uint32_t hashIndex;
    timestamp_t timestamp;
    char *data;
} blmSlice;

struct ageBloom_s {
    // Low level variables
    uint32_t numHash;         // k_a - Number of Hash functions used
    uint32_t batches;         // l   - Number of batches to store
    uint32_t optimalSlices;   // k+l - Optimal number of slices

    // High level variables
    double errorRate;         // Required error rate
    uint64_t capacity;        // Capacity required by user
    uint64_t inserts;         // Items already inserted

    // Time based variables
    uint32_t numSlices;       // Current number of slices
    uint32_t assessFreq;      // Frequency of assessment
    uint32_t maxUpdates;      // Maximum updates the slice[updatesIndex] can have
    uint32_t updates;         // Updates until next shift
    uint32_t updatesIndex;    // Index of the slice with the minimum updates

    blmSlice *slices;
};

/*
 *  Return an Age-Partitioned Bloom-Filter for `capacity` number of items and
 *  `bitsPerItem`.
 *
 *  Number of hash functions is :             ceil(log2(2) * bitsPerItem)
 *  Number of `timeframes` is at minimum :    (hashFunctions * 2)
 *  `errorRate` can be used for maintainance when switching a timeframe.
 */
ageBloom_t* APBF_createLowLevelAPI(uint32_t numHash, uint32_t timeframes, uint64_t sliceSize);
ageBloom_t* APBF_createHighLevelAPI(int error, uint64_t capacity, int8_t level);
ageBloom_t* APBF_createTimeAPI(int error, __uint64_t capacity, __int8_t level, timestamp_t frequency);

/*
 *  Free all resources allocated to `apbf`.
 */
void APBF_destroy(ageBloom_t *apbf);

/*
 *  Insert an `item` into `apbf`.
 */
void APBF_insert(ageBloom_t *apbf, const char *item, uint32_t itemlen);
void APBF_insertTime(ageBloom_t *apbf, const char *item, uint32_t itemlen);
void APBF_insertCount(ageBloom_t *apbf, const char *item, uint32_t itemlen);

/*
 *  Check if an item exists in the filter.
 *  `timeframes` will limit the frames to check. CHECK_ALL will check all available timeframes.
 */
bool APBF_query(ageBloom_t *apbf, const char *item, uint32_t itemlen/*, uint32_t timeframes*/);

/*
 *  Print bit representation of the filter with basic metrics.
 */
void APBF_print(ageBloom_t *apbf);

/*
 *  Return number of hash functions used.
 */
uint32_t APBF_getHashNum(ageBloom_t *apbf);

/*
 *  Return total size taken by filter
 */
uint64_t APBF_size(ageBloom_t *apbf);
uint64_t APBF_totalSize(ageBloom_t *apbf);
uint64_t APBF_filterCap(ageBloom_t *apbf);


#endif  //  __H_FLEX_BLOOM__

//  Checking first the center one since it is crucial.
