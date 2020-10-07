/*
 * Copyright 2019-2020 Redis Labs Ltd. and Contributors
 *
 * This file is available under the Redis Labs Source Available License Agreement
 *
 * The Bloom-Filter variant is based on the paper "Age-Partitioned Bloom Filters"
 * by Paulo Sérgio Almeida, Carlos Baquero and Ariel Shtul
 * Link: https://arxiv.org/pdf/2001.03147.pdf
 */

#include <math.h>       // log, ceil, floor
#include <stdio.h>      // printf
#include <assert.h>     // assert
#include <stdlib.h>     // calloc
#include <string.h>     // memset
#include <unistd.h>     // write
#include <sys/time.h>   // gettimeofday

#include "hash.h"
#include "agingBloom.h"

#define _64BITS 64
#define BYTE 8

struct timeval start, end;  // Used to record time between insertions.
timestamp_t ts;             // Timestamp in seconds.

/******************* Static functions ***************/
static void SetBitOn(char *array, uint64_t loc) {
    //  Check for Little/Big Endian
    char *byte = &array[loc / BYTE];
    uint8_t bit = loc % BYTE;
    *byte |= 1 << bit;
}

static bool CheckBitOn(char *array, uint64_t loc) {
    //  Check for Little/Big Endian
    char *byte = &array[loc / BYTE];
    uint8_t bit = loc % BYTE;
    return *byte & (1 << bit);
}

static bool CheckSetBitOn(char *array, uint64_t loc) {
    //  Check for Little/Big Endian
    char *byte = &array[loc / BYTE];
    uint8_t bit = loc % BYTE;
    bool result = *byte & (1 << bit);
    *byte |= 1 << bit;
    return result;
}

static uint64_t CountBitOn(uint64_t barr) {
    //  Consider using LUT though can do 8 bit each call
    uint64_t result = 0;
    while(barr)
    {
        barr &= (barr - 1);
        ++result;
    }
    return result;
}

static double CalcBitPerItem(double error) {
    assert(error > 0 && error < 1);

    return -log(error) / pow(log(2),2);
}

static uint64_t max(uint64_t a, uint64_t b) {
    return a > b ? a : b;
}

static uint64_t ceil64(uint64_t n) {
    return ceil(n / (double)_64BITS);
}

/*****************************************************************************/
/*********                    blmSlice functions                    **********/
/*****************************************************************************/
static blmSlice createSlice(uint64_t size, uint32_t hashIndex,
                            timestamp_t timestamp) {
    blmSlice slice = {.size = size,
            .count = 0,
            .hashIndex = hashIndex,
            .timestamp = timestamp };
    slice.data = (char *)calloc(ceil64(size), sizeof(uint64_t));
    return slice;
}

static void destroySlice(blmSlice *slice) {
    free(slice->data);
    slice->data = NULL;
}

/*****************************************************************************/
/*********                      Help functions                      **********/
/*****************************************************************************/

// Shifting slices is done by retiring the last slice, moving all slices down
// the slices array and inserting a new slice at location 0.
// hashIndex for the new slice is calculated slice at location 1.
static void APBF_shiftSlice(ageBloom_t *apbf) {
    assert (apbf);

    uint32_t numHash = apbf->numHash;
    uint32_t numSlices = apbf->numSlices;
    blmSlice *slices = apbf->slices;

    char *data = slices[numSlices - 1].data;

    memmove(slices + 1, slices, sizeof(blmSlice) * (numSlices - 1));

    slices[0].hashIndex = (apbf->slices[1].hashIndex - 1 + numHash) % numHash;
    slices[0].data = data;
    memset(data, 0, slices[0].size / BYTE);
}

// Used for time based APBF
// Retires last slice, moves all slices down the slices array
// and inserts a new slice at location 0 with specified size.
static void APBF_shiftTimeSlice(ageBloom_t *apbf, uint64_t size) {
    assert (apbf);

    uint32_t numHash = apbf->numHash;
    uint32_t numSlices = apbf->numSlices;
    blmSlice *slices = apbf->slices;

    char *data = slices[numSlices - 1].data;
    memset(data, 0, slices[numSlices - 1].size / BYTE);
    data = (char *)realloc(data, sizeof(uint64_t) * ceil64(size)); // realloc to new size
    assert(data);

    memmove(slices + 1, slices, sizeof(blmSlice) * (numSlices - 1));

    slices[0].count = 0;
    slices[0].timestamp = 0;
    slices[0].hashIndex = (apbf->slices[1].hashIndex - 1 + numHash) % numHash;
    slices[0].data = data;
    slices[0].size = size;
}


/*****************************************************************************/
/*********                          Oracle                          **********/
/*****************************************************************************/

// Retires slices that have expired (older than timestamp).
static void retireSlices(ageBloom_t *apbf) {
    assert (apbf);

    blmSlice *slices = apbf->slices;
    int32_t i;

    // try retiring slices older than timestamp. Must leave optimal minimal number.
    for(i = apbf->numSlices - 1; i > apbf->optimalSlices - 1; --i) {
        if (slices[i].timestamp < ts - apbf->assessFreq) {
            destroySlice(&slices[i]);
            continue;
        }
        break;
    }

    // if at least one slice was retired, realloc slices' memory space.
    if (i < apbf->numSlices - 1) {
        apbf->numSlices = ++i;
        slices = (blmSlice *)realloc(slices, sizeof(blmSlice) * i);
        assert(slices);
        apbf->slices = slices;
    }
}

// Calculates the new size of slice 0.
static uint64_t predictSize(ageBloom_t *apbf) {
    assert(apbf);

    uint32_t numHash = apbf->numHash;
    blmSlice *slices = apbf->slices;

    double_t tbs = slices[numHash - 1].timestamp - slices[numHash].timestamp; // time since last shift
    double_t targetTime = (double_t) apbf->assessFreq / apbf->batches; // target time between shifts

    double_t efr = (double_t) 1 / (2 * numHash); // expected fill ratio of slice 0
    double_t generation = (double_t) (apbf->updates * targetTime) / tbs; // target updates to make between shifts
    double_t size = - ((ceil(generation) + 1) / log(1 - efr));

    // Size must be larger than/equal to numHash * 2 for the slice to allow at least one insertion.
    size = (size < numHash * 2) ? numHash * 2 : size;

    return ceil(size);
}

// Adds an additional time frame with specified size.
static void APBF_addTimeframe(ageBloom_t *apbf, uint64_t size) {
    assert (apbf);

    uint32_t numHash = apbf->numHash;
    blmSlice *slices = apbf->slices;

    // realloc slices' memory space
    slices = (blmSlice *)realloc(slices, sizeof(blmSlice) * (++apbf->numSlices));
    assert(slices);

    memmove(slices + 1, slices, sizeof(blmSlice) * (apbf->numSlices - 1));

    uint32_t hashIndex = (slices[1].hashIndex - 1 + numHash) % numHash;
    slices[0] = createSlice(size, hashIndex, 0); // creates the new slice at location 0

    apbf->slices = slices;
}

// Calculates how many updates are allowed until the next shift.
static void updateShiftTrigger(ageBloom_t *apbf) {
    assert(apbf);

    uint32_t numHash = apbf->numHash;
    blmSlice *slices = apbf->slices;
    uint32_t maxUpdates = 0, updates = UINT32_MAX, updatesIndex = 0;

    for (int32_t i = 0; i < numHash; ++i) {
        // Maximum updates the slice can store until it becomes full
        uint32_t maxUpdatesSlice = floor(log(1 - ((i + 1.0) / (2 * numHash))) / log(1 - (1.0 / slices[i].size)));
        // Updates the slice can still store
        uint32_t updatesSlice = maxUpdatesSlice - slices[i].count;

        if (updatesSlice < updates) {
            maxUpdates = maxUpdatesSlice;
            updates = updatesSlice;
            updatesIndex = i;
        }
    }

    apbf->maxUpdates = maxUpdates;
    apbf->updates = updates;
    apbf->updatesIndex = updatesIndex;
}

// Increments time.
static void updateTimestamp() {
    gettimeofday(&end, NULL);
    uint32_t seconds = end.tv_sec - start.tv_sec;
    uint64_t micros = seconds * 1000000 + end.tv_usec - start.tv_usec;
    ts += (double_t) micros / 1000000;
    start = end;
}

/*****************************************************************************/
/*********                      APBF functions                      **********/
/*****************************************************************************/

ageBloom_t* APBF_createLowLevelAPI(uint32_t numHash, uint32_t timeframes, uint64_t sliceSize) {
    ageBloom_t *apbf = (ageBloom_t *)calloc(1, sizeof(ageBloom_t));
    if(apbf == NULL) { return NULL; }

    apbf->numHash = numHash;
    apbf->batches = timeframes;
    apbf->numSlices = apbf->optimalSlices = timeframes + numHash;

    apbf->slices = (blmSlice *)calloc(apbf->numSlices, sizeof(blmSlice));
    if (apbf->slices == NULL) {
        free(apbf);
        return NULL;
    }

    for(uint32_t i = 0; i < apbf->numSlices; ++i) {
        apbf->slices[i].size = sliceSize;
        apbf->slices[i].hashIndex = i % numHash;
        apbf->slices[i].data = (char *)calloc(ceil64(sliceSize), sizeof(uint64_t));

        if (apbf->slices[i].data == NULL) {
            while(i > 0) {
                free(apbf->slices[--i].data);
            }
            free(apbf->slices);
            free(apbf);
            return NULL;
        }
    }

    return apbf;
}

ageBloom_t* APBF_createHighLevelAPI(int error, uint64_t capacity, int8_t level) {
    uint64_t k = 0;
    uint64_t l = 0;

    // `error` is is form of 10^(-error). ex. error = 1 -> 0.1, error = 3 -> 0.001
    // These values are precalculated and appear in the paper.
    // They optimize the number of slides for a given number of hash functions.
    switch (error)
    {
        case 1:
            switch (level) {
                case 0: k = 4; l = 3; break;
                case 1: k = 5; l = 7; break;
                case 2: k = 6; l = 14; break;
                case 3: k = 7; l = 28; break;
                case 4: k = 8; l = 56; break;
                default: break;
            }
            break;
        case 2:
            switch (level) {
                case 0: k = 7; l = 5; break;
                case 1: k = 8; l = 8; break;
                case 2: k = 9; l = 14; break;
                case 3: k = 10; l = 25; break;
                case 4: k = 11; l = 46; break;
                case 5: k = 12; l = 88; break;
                default: break;
            }
            break;
        case 3:
            switch (level) {
                case 0: k = 10; l = 7; break;
                case 1: k = 11; l = 9; break;
                case 2: k = 12; l = 14; break;
                case 3: k = 13; l = 23; break;
                case 4: k = 14; l = 40; break;
                case 5: k = 15; l = 74; break;
                default: break;
            }
            break;
        case 4:
            switch (level) {
                case 0: k = 14; l = 11; break;
                case 1: k = 15; l = 15; break;
                case 2: k = 16; l = 22; break;
                case 3: k = 17; l = 36; break;
                case 4: k = 18; l = 63; break;
                case 5: k = 19; l = 117; break;
                default: break;
            }
            break;
        case 5:
            switch (level) {
                case 0: k = 17; l = 13; break;
                case 1: k = 18; l = 16; break;
                case 2: k = 19; l = 22; break;
                case 3: k = 20; l = 33; break;
                case 4: k = 21; l = 54; break;
                case 5: k = 22; l = 95; break;
                default: break;
            }
        default: break;
    }

    assert(capacity > l); // To avoid a mod 0 on the shift code
    uint64_t sliceSize = (capacity * k) / (l * log(2));

    ageBloom_t* bf = APBF_createLowLevelAPI(k, l, ceil(sliceSize));
    bf->capacity = capacity;
    bf->errorRate = error;
    return bf;
}

// Used for time based APBF
ageBloom_t *APBF_createTimeAPI(int error, uint64_t capacity, int8_t level, timestamp_t frequency) {
    ageBloom_t *apbf = APBF_createHighLevelAPI(error, capacity, level);
    assert(apbf);
    ts = apbf->assessFreq = frequency;
    apbf->slices[apbf->numHash].timestamp = ts;
    updateShiftTrigger(apbf);
#ifndef SLEEP
    gettimeofday(&start, NULL);
#endif
    return apbf;
}

void APBF_destroy(ageBloom_t *apbf) {
    assert(apbf);

    for(uint32_t i = 0; i < apbf->numSlices; ++i) {
        free(apbf->slices[i].data);
    }

    free(apbf->slices);
    apbf->slices = NULL;

    free(apbf);
}

void APBF_insert(ageBloom_t *apbf, const char *item, uint32_t itemlen) {
    assert(apbf);
    assert(item);

#ifdef TWOHASH
    TwoHash_t twoHash;
    getTwoHash(&twoHash, item, itemlen);
    for(uint32_t i = 0; i < apbf->numHash; ++i) {
        uint64_t hash = getHash(twoHash, apbf->slices[i].hashIndex);
        SetBitOn(apbf->slices[i].data, hash % apbf->slices[i].size);
        ++apbf->slices[i].count;
    }
#else
    for(uint32_t i = 0; i < apbf->numHash; ++i) {
        uint64_t hash = MurmurHash64A_Bloom(item, itemlen, apbf->slices[i].hashIndex);
        SetBitOn(apbf->slices[i].data, hash % apbf->slices[i].size);
        ++apbf->slices[i].count;
    }
#endif
    ++apbf->inserts;
}

// Used for time based APBF
void APBF_insertTime(ageBloom_t *apbf, const char *item, uint32_t itemlen) {
    assert(apbf);
    assert(item);

#ifndef SLEEP
    updateTimestamp();
#endif

    // Checking whether it is time to shift a time frame.
    if (apbf->slices[apbf->updatesIndex].count == apbf->maxUpdates) {
        retireSlices(apbf);
        uint64_t size = predictSize(apbf);

        uint32_t numSlices = apbf->numSlices;
        blmSlice *slices = apbf->slices;

        // Checking whether a new time frame is needed (if last slice isn't older than timestamp).
        if (slices[numSlices - 1].timestamp >= ts - apbf->assessFreq && slices[numSlices - 1].count) {
            APBF_addTimeframe(apbf, size);
        } else {
            APBF_shiftTimeSlice(apbf, size);
        }

        updateShiftTrigger(apbf);
    }
    APBF_insert(apbf, item, itemlen);

#ifdef SLEEP
    ts += SLEEP;
#endif

    // Checking whether next update will trigger a shift.
    if (apbf->slices[apbf->updatesIndex].count == apbf->maxUpdates) {
        apbf->slices[apbf->numHash - 1].timestamp = ts; // save timestamp of last insertion
    }
}

// Used for count based APBF
void APBF_insertCount(ageBloom_t *apbf, const char *item, uint32_t itemlen) {
    // TODO: in future which slice size?
    // Checking whether it is time to shift a timeframe.
    if ((apbf->inserts + 1) % (uint64_t)(apbf->slices[0].size * log(2) / apbf->numHash) == 0) {
        APBF_shiftSlice(apbf);
    }
    APBF_insert(apbf, item, itemlen);
}

// Query algorithm is described in the paper.
// It optimizes the process by checking only locations with a potential of `k`
// continuos on bits which will yield a positive result.
bool APBF_query(ageBloom_t *apbf, const char *item, uint32_t itemlen/*, uint *age */) {
    assert(apbf);
    assert(item);

    uint64_t hashArray[apbf->numSlices];
    memset(hashArray, 0x0, apbf->numSlices * sizeof(uint64_t));

    uint64_t hash;
    uint32_t done = 0;
    uint32_t carry = 0;
    int32_t cursor = apbf->numSlices - apbf->numHash;
#ifdef TWOHASH
    TwoHash_t twoHash;
    getTwoHash(&twoHash, item, itemlen);
#endif

    while (cursor >= 0) {
        uint32_t hashIdx = apbf->slices[cursor].hashIndex;

        // Calculated hash values are saved into `hashArray` for reuse
        if (hashArray[hashIdx] != 0) {
            hash = hashArray[hashIdx];
        } else {
#ifdef TWOHASH
            hash = hashArray[hashIdx] = getHash(twoHash, hashIdx);
#else
            hash = hashArray[hashIdx] = MurmurHash64A_Bloom(item, itemlen, hashIdx);
#endif
        }

        if (CheckBitOn(apbf->slices[cursor].data, hash % apbf->slices[cursor].size)) {
            done++;
            if (done + carry == apbf->numHash) {
                return true;
            }
            cursor++;
        } else {
            cursor -= apbf->numHash;
            carry = done;
            done = 0;
        }
    }

    return false;
}

/*********************** Debug functions ********************************/
/*
static void PrintBinary(uint64_t n) {
    int iPos = 0;
    uint64_t i = 1; // why not 1 at in if?
    for (iPos = (_64BITS - 1) ; iPos >= 0; --iPos) {
      (n & (i << iPos))? printf("1"): printf("0");
    }
}
static void PrintFilter(blmSlice *filter) {
  for(uint64_t i = 0; i < ceil64(filter->size); ++i) {
    PrintBinary(*((uint64_t *)filter->data + i));
  }
  printf("\n");
}
void APBF_print(ageBloom_t *apbf) {
  printf("Filter variables - hashes %d, timeframes %d, slices %d, bits per slice %ld\n",
          apbf->numHash, apbf->timeframes, apbf->numSlices, apbf->minFilterCap);
  for(uint32_t i = 0; i < apbf->numSlices; ++i) {
    PrintFilter(&apbf->slices[i]);
  }
}
uint32_t APBF_getHashNum(ageBloom_t *apbf) {
  return apbf->numHash;
}
uint64_t APBF_size(ageBloom_t *apbf) {
  return sizeof(ageBloom_t) + apbf->numSlices * sizeof(blmSlice) +
         apbf->numSlices * ceil64(apbf->slices[0].size) * sizeof(uint64_t);
}
uint64_t APBF_totalSize(ageBloom_t *apbf) {
  return APBF_size(apbf);
}
uint64_t APBF_filterCap(ageBloom_t *apbf) {
  return apbf->slices[0].capacity;
}
*/