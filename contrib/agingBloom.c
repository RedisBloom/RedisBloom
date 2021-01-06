/*
 * Copyright 2019-2020 Redis Labs Ltd. and Contributors
 *
 * This file is available under the Redis Labs Source Available License Agreement
 *
 * The Bloom-Filter variant is based on the paper "Age-Partitioned Bloom Filters"
 * by Paulo Sérgio Almeida, Carlos Baquero and Ariel Shtul
 * Link: https://arxiv.org/pdf/2001.03147.pdf
 */

#include <math.h>   // log, ceil
#include <stdio.h>  // printf
#include <assert.h> // assert
#include <stdlib.h> // calloc
#include <string.h> // memset
#include <unistd.h> // write

#include "hash.h"
#include "agingBloom.h"

#define _64BITS 64
#define BYTE 8


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
    while (barr) {
        barr &= (barr - 1);
        ++result;
    }
    return result;
}

static double CalcBitPerItem(double error) {
    assert(error > 0 && error < 1);

    return -log(error) / pow(log(2), 2);
}

static uint64_t max(uint64_t a, uint64_t b) { return a > b ? a : b; }

static uint64_t ceil64(uint64_t n) { return ceil(n / (double)_64BITS); }

/*****************************************************************************/
/*********                    blmSlice functions                    **********/
/*****************************************************************************/
static blmSlice createSlice(uint64_t size, uint32_t hashIndex, timestamp_t timestamp) {
    blmSlice slice = {.size = size, .count = 0, .hashIndex = hashIndex, .timestamp = timestamp};
    slice.data = (char *)calloc(ceil64(size), sizeof(uint64_t));
    return slice;
}

static void destroySlice(blmSlice *slice) {
    free(slice->data);
    slice->data = NULL;
}

static uint64_t sliceCapacity(blmSlice *slice) {
    return log(2) * slice->size;
}

/*****************************************************************************/
/*********                      Help functions                      **********/
/*****************************************************************************/

// Shifting slices is done by retiring the last slice, moving all slices down
// the slices array and inserting a new slice at location 0.
// hashIndex for the new slice is calculated slice at location 1.
static void APBF_shiftSliceCount(ageBloom_t *apbf) {
    assert (apbf);

    uint16_t numHash = apbf->numHash;
    uint32_t numSlices = apbf->numSlices;
    blmSlice *slices = apbf->slices;

    char *data = slices[numSlices - 1].data;

    memmove(slices + 1, slices, sizeof(blmSlice) * (numSlices - 1));

    slices[0].hashIndex = (apbf->slices[1].hashIndex - 1 + numHash) % numHash;
    slices[0].data = data;
    memset(data, 0, slices[0].size / BYTE);
}

// Used for time based APBF.
// Retires last slice (if it's older than timestamp) and moves all slices down
// the slices array. Doesn't insert a new slice at location 0.
static void APBF_shiftTimeSlice(ageBloom_t *apbf) {
    assert (apbf);

    uint32_t numSlices = apbf->numSlices;
    blmSlice *slices = apbf->slices;

    // Checking whether a new time frame will be needed (if last slice isn't older than timestamp).
    if ((slices[numSlices - 1].timestamp >= apbf->currTime - apbf->timeSpan) && slices[numSlices - 1].count) {
        // realloc slices' memory space
        numSlices = ++apbf->numSlices;
        slices = (blmSlice *)realloc(slices, sizeof(blmSlice) * numSlices);
        assert(slices);
        apbf->slices = slices;
    }
    else {
        destroySlice(&slices[numSlices - 1]);
    }

    memmove(slices + 1, slices, sizeof(blmSlice) * (numSlices - 1));
}

/*****************************************************************************/
/*********                          Oracle                          **********/
/*****************************************************************************/

// Retires slices that have expired (older than timestamp).
static void retireSlices(ageBloom_t *apbf) {
    assert (apbf);

    blmSlice *slices = apbf->slices;
    uint32_t i;

    // try retiring slices older than timestamp. Must leave optimal minimal number.
    for(i = apbf->numSlices - 1; i > apbf->optimalSlices - 1; --i) {
        if (slices[i].timestamp < apbf->currTime - apbf->timeSpan) {
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

// Adds a new slice at location 0 with specified size.
static void APBF_addTimeframe(ageBloom_t *apbf, uint64_t size) {
    assert (apbf);

    uint16_t numHash = apbf->numHash;
    blmSlice *slices = apbf->slices;

    uint32_t hashIndex = (slices[1].hashIndex - 1 + numHash) % numHash;
    slices[0] = createSlice(size, hashIndex, 0);
}

// Calculates how many updates are allowed until the next shift.
// To be invoked after a shift, after adding the new slice 0.
static void nextGenerationSize(ageBloom_t *apbf) {
    assert(apbf);

    blmSlice *slices = apbf->slices;
    uint16_t numHash = apbf->numHash;
    uint64_t minGeneration = INT64_MAX;

    for (uint16_t i = 0; i < numHash; ++i) {
        uint64_t generation = ceil((double_t) (sliceCapacity(&slices[i]) - slices[i].count) / (numHash - i));

        if (generation < minGeneration) {
            minGeneration = generation;
        }
    }

    apbf->counter = apbf->genSize = minGeneration;
}

// Calculates the target generation size, i.e., how many updates
// we should aim to make between shifts.
static uint64_t targetGenerationSize(ageBloom_t *apbf) {
    assert(apbf);

    uint16_t numHash = apbf->numHash;
    blmSlice *slices = apbf->slices;

    uint64_t tbs = slices[numHash - 1].timestamp - slices[numHash].timestamp; // time since last shift
    double_t targetTime = (double_t) apbf->timeSpan / apbf->batches; // target time between shifts

    uint64_t generation = ceil((apbf->genSize * targetTime) / tbs);

    return generation + 1;
}

// Calculates the new size for slice 0; to be invoked after a shift, before addding a new slice 0.
// Does it calculating how many insertions can be done at most, given current slices from 1 to k-1
// until (if ever) the new slice 0 becomes the new limit, and adds it to the product
// of the new target generation times the remaining shifts.
static uint64_t predictSize(ageBloom_t *apbf, uint64_t targetGeneration) {
    assert(apbf);

    blmSlice *slices = apbf->slices;
    uint16_t numHash;
    uint16_t i = numHash = apbf->numHash;
    uint64_t count = 0;

    while (i > 1) {
        uint64_t minGeneration = INT64_MAX;
        uint32_t j_min = i;

        for (uint32_t j = i - 1; j > 0; --j) {
            uint64_t generation = ceil((double_t) (sliceCapacity(&slices[j]) - slices[j].count - count) / (i - j));

            if (generation <= minGeneration) {  // <= keeps the smallest j in case of draw
                minGeneration = generation;
                j_min = j;
            }
        }

        if (targetGeneration <= minGeneration) {
            break;
        }

        count += minGeneration * (i - j_min);
        i = j_min;
    }

    uint64_t capacity = count + i * targetGeneration;
    uint64_t size = ceil(capacity / log(2));

    // Size must be larger than/equal to numHash * 2 for the slice to allow at least one insertion.
    size = (size < numHash * 2) ? numHash * 2 : size;

    return size;
}

// Increments time and records the timestamp of the last insertion.
static void updateTimestamp(ageBloom_t *apbf) {
    assert(apbf);

    timeval start = apbf->lastTimestamp;
    timeval end;

    gettimeofday(&end, NULL);
    uint32_t seconds = end.tv_sec - start.tv_sec;
    uint64_t micros = seconds * 1000000 + end.tv_usec - start.tv_usec;
    uint64_t millis = micros / 1000;

    apbf->currTime += millis;
    apbf->lastTimestamp = end;
}

/*****************************************************************************/
/*********                      APBF functions                      **********/
/*****************************************************************************/

ageBloom_t *APBF_createLowLevelAPI(uint16_t numHash, uint16_t timeframes, uint64_t sliceSize) {
    ageBloom_t *apbf = (ageBloom_t *)calloc(1, sizeof(ageBloom_t));
    if (apbf == NULL) {
        return NULL;
    }

    apbf->numHash = numHash;
    apbf->batches = timeframes;
    apbf->numSlices = apbf->optimalSlices = timeframes + numHash;

    apbf->slices = (blmSlice *)calloc(apbf->numSlices, sizeof(blmSlice));
    if (apbf->slices == NULL) {
        free(apbf);
        return NULL;
    }

    for (uint32_t i = 0; i < apbf->numSlices; ++i) {
        apbf->slices[i].size = sliceSize;
        apbf->slices[i].hashIndex = i % numHash;
        apbf->slices[i].data = (char *)calloc(ceil64(sliceSize), sizeof(uint64_t));

        if (apbf->slices[i].data == NULL) {
            while (i > 0) {
                free(apbf->slices[--i].data);
            }
            free(apbf->slices);
            free(apbf);
            return NULL;
        }
    }

    return apbf;
}

ageBloom_t *APBF_createHighLevelAPI(int error, uint64_t capacity, int8_t level) {
    uint64_t k = 0;
    uint64_t l = 0;

    // `error` is is form of 10^(-error). ex. error = 1 -> 0.1, error = 3 -> 0.001
    // These values are precalculated and appear in the paper.
    // They optimize the number of slides for a given number of hash functions.
    switch (error) {
    case 1:
        switch (level) {
        case 0:
            k = 4;
            l = 3;
            break;
        case 1:
            k = 5;
            l = 7;
            break;
        case 2:
            k = 6;
            l = 14;
            break;
        case 3:
            k = 7;
            l = 28;
            break;
        case 4:
            k = 8;
            l = 56;
            break;
        default:
            break;
        }
        break;
    case 2:
        switch (level) {
        case 0:
            k = 7;
            l = 5;
            break;
        case 1:
            k = 8;
            l = 8;
            break;
        case 2:
            k = 9;
            l = 14;
            break;
        case 3:
            k = 10;
            l = 25;
            break;
        case 4:
            k = 11;
            l = 46;
            break;
        case 5:
            k = 12;
            l = 88;
            break;
        default:
            break;
        }
        break;
    case 3:
        switch (level) {
        case 0:
            k = 10;
            l = 7;
            break;
        case 1:
            k = 11;
            l = 9;
            break;
        case 2:
            k = 12;
            l = 14;
            break;
        case 3:
            k = 13;
            l = 23;
            break;
        case 4:
            k = 14;
            l = 40;
            break;
        case 5:
            k = 15;
            l = 74;
            break;
        default:
            break;
        }
        break;
    case 4:
        switch (level) {
        case 0:
            k = 14;
            l = 11;
            break;
        case 1:
            k = 15;
            l = 15;
            break;
        case 2:
            k = 16;
            l = 22;
            break;
        case 3:
            k = 17;
            l = 36;
            break;
        case 4:
            k = 18;
            l = 63;
            break;
        case 5:
            k = 19;
            l = 117;
            break;
        default:
            break;
        }
        break;
    case 5:
        switch (level) {
        case 0:
            k = 17;
            l = 13;
            break;
        case 1:
            k = 18;
            l = 16;
            break;
        case 2:
            k = 19;
            l = 22;
            break;
        case 3:
            k = 20;
            l = 33;
            break;
        case 4:
            k = 21;
            l = 54;
            break;
        case 5:
            k = 22;
            l = 95;
            break;
        default:
            break;
        }
    default:
        break;
    }

    assert(capacity > l); // To avoid a mod 0 on the shift code
    double_t sliceSize = (double_t) (capacity * k) / (l * log(2));

    ageBloom_t *apbf = APBF_createLowLevelAPI(k, l, ceil(sliceSize));
    apbf->capacity = capacity;
    apbf->errorRate = error;

    return apbf;
}

// Used for time based APBF.
ageBloom_t *APBF_createTimeAPI(int error, uint64_t capacity, int8_t level, timestamp_t timeSpan) {
    ageBloom_t *apbf = APBF_createHighLevelAPI(error, capacity, level);
    assert(apbf);
    // convert timeSpan from seconds to milliseconds
    apbf->slices[apbf->numHash].timestamp = apbf->currTime = apbf->timeSpan = timeSpan * 1000;
    nextGenerationSize(apbf);
    gettimeofday(&apbf->lastTimestamp, NULL);
    return apbf;
}

void APBF_destroy(ageBloom_t *apbf) {
    assert(apbf);

    for (uint32_t i = 0; i < apbf->numSlices; ++i) {
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
    for (uint16_t i = 0; i < apbf->numHash; ++i) {
        uint64_t hash = getHash(twoHash, apbf->slices[i].hashIndex);
        SetBitOn(apbf->slices[i].data, hash % apbf->slices[i].size);
        ++apbf->slices[i].count;
    }
#else
    for (uint16_t i = 0; i < apbf->numHash; ++i) {
        uint64_t hash = MurmurHash64A_Bloom(item, itemlen, apbf->slices[i].hashIndex);
        SetBitOn(apbf->slices[i].data, hash % apbf->slices[i].size);
        ++apbf->slices[i].count;
    }
#endif
    ++apbf->inserts;
}

void APBF_slideTime(ageBloom_t *apbf) {
    updateTimestamp(apbf);
    uint64_t targetGenSize = targetGenerationSize(apbf);
    retireSlices(apbf);
    APBF_shiftTimeSlice(apbf);
    uint64_t size = predictSize(apbf, targetGenSize);
    APBF_addTimeframe(apbf, size);
    nextGenerationSize(apbf);
}

// Used for time based APBF.
void APBF_insertTime(ageBloom_t *apbf, const char *item, uint32_t itemlen) {
    assert(apbf);
    assert(item);

    // Checking whether it is time to shift a time frame.
    if (apbf->counter == 0) {
        APBF_slideTime(apbf);
    }
    APBF_insert(apbf, item, itemlen);
    --apbf->counter;

    // Checking whether next insertion will trigger a shift.
    if (apbf->counter == 0) {
        updateTimestamp(apbf);
        apbf->slices[apbf->numHash - 1].timestamp = apbf->currTime; // save timestamp of last insertion
    }
}

// Used for count based APBF
void APBF_insertCount(ageBloom_t *apbf, const char *item, uint32_t itemlen) {
    // TODO: in future which slice size?
    // Checking whether it is time to shift a timeframe.
    if ((apbf->inserts + 1) % (uint64_t)(apbf->slices[0].size * log(2) / apbf->numHash) == 0) {
        APBF_shiftSliceCount(apbf);
    }
    APBF_insert(apbf, item, itemlen);
}

// Query algorithm is described in the paper.
// It optimizes the process by checking only locations with a potential of `k`
// continuos on bits which will yield a positive result.
bool APBF_query(ageBloom_t *apbf, const char *item, uint32_t itemlen /*, uint *age */) {
    assert(apbf);
    assert(item);

    uint64_t hashArray[apbf->numSlices];
    memset(hashArray, 0x0, apbf->numSlices * sizeof(uint64_t));

    uint64_t hash;
    uint32_t done = 0;
    uint32_t carry = 0;
    int32_t cursor = apbf->numSlices - apbf->numHash;
    blmSlice *slices = apbf->slices;
#ifdef TWOHASH
    TwoHash_t twoHash;
    getTwoHash(&twoHash, item, itemlen);
#endif

    updateTimestamp(apbf);

    while (cursor >= 0) {
        uint32_t hashIdx = slices[cursor].hashIndex;

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

        // Checking whether a slice has expired (older than timestamp). If so, it's not used for query testing.
        bool doCheck = true;
        if (slices[cursor].timestamp && (slices[cursor].timestamp < apbf->currTime - apbf->timeSpan)) {
            doCheck = false;
        }

        if (doCheck && CheckBitOn(slices[cursor].data, hash % slices[cursor].size)) {
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
