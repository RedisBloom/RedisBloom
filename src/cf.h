/*
 * Copyright (c) 2006-Present, Redis Ltd.
 * All rights reserved.
 *
 * Licensed under your choice of (a) the Redis Source Available License 2.0
 * (RSALv2); or (b) the Server Side Public License v1 (SSPLv1); or (c) the
 * GNU Affero General Public License v3 (AGPLv3).
 */

#ifndef CF_H
#define CF_H
#include "cuckoo.h"
#include <stddef.h>

const char *CF_GetEncodedChunk(const CuckooFilter *cf, long long *pos, size_t *buflen,
                               size_t bytelimit);
int CF_LoadEncodedChunk(const CuckooFilter *cf, long long pos, const char *data, size_t datalen);

/* Dump-only flag in the 64-bit numFiltersAndFlags field. Real counts fit in uint16_t,
 * so valid legacy headers never set this bit. Use a mask, not C bit-fields,
 * whose bit ordering is implementation-defined. The in-memory count is unchanged. */
#define CF_DUMP_HAS_SEED (UINT64_C(1) << 63)
#define CF_DUMP_NUM_FILTERS_MASK UINT64_C(0x000000000000ffff)
#define CF_DUMP_RESERVED_MASK UINT64_C(0x7fffffffffff0000)

typedef struct __attribute__((packed)) {
    uint64_t numItems;
    uint64_t numBuckets;
    uint64_t numDeletes;
    /* Bits 0-15: filter count. Bits 16-62: reserved, must be zero.
     * Bit 63: hasSeed (CF_DUMP_HAS_SEED), requires the trailing eight-byte seed. */
    uint64_t numFiltersAndFlags;
    uint16_t bucketSize;
    uint16_t maxIterations;
    uint16_t expansion;
    uint64_t seed;
} CFHeader;

CuckooFilter *CFHeader_Load(const CFHeader *header, size_t len);
CFHeader fillCFHeader(const CuckooFilter *cf);

#endif
