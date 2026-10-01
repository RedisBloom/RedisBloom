/* Run with gmake unit-tests. Keep assertions active in release builds. */
#undef NDEBUG
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "redismodule.h"
#include "cms.h"
#include "murmur2/murmurhash2.h"

static uint64_t getCell(const CMSketch *cms, size_t loc) {
    switch (cms->cellSize) {
    case 1: return ((uint8_t *)cms->array)[loc];
    case 2: return ((uint16_t *)cms->array)[loc];
    case 4: return ((uint32_t *)cms->array)[loc];
    default: return ((uint64_t *)cms->array)[loc];
    }
}

static void setCell(CMSketch *cms, size_t loc, uint64_t value) {
    switch (cms->cellSize) {
    case 1: ((uint8_t *)cms->array)[loc] = value; break;
    case 2: ((uint16_t *)cms->array)[loc] = value; break;
    case 4: ((uint32_t *)cms->array)[loc] = value; break;
    default: ((uint64_t *)cms->array)[loc] = value; break;
    }
}

int main(void) {
    RedisModule_Calloc = calloc;
    RedisModule_Free = free;
    const uint32_t seeds[] = {0, 123, UINT32_C(0x80000000), UINT32_MAX};
    for (uint8_t size = 1; size <= 8; size *= 2) {
        for (size_t s = 0; s < sizeof(seeds) / sizeof(*seeds); ++s) {
            CMSketch *cms = NewCMSketch(127, 5, size);
            assert(cms && cms->seed == 0);
            cms->seed = seeds[s];
            uint64_t reference[127 * 5] = {0};
            for (unsigned i = 0; i < 100; ++i) {
                char item[] = {(char)i, '\0', (char)0xff};
                uint64_t count;
                /* Include an empty item and embedded NUL/high-bit bytes. */
                size_t len = i == 0 ? 0 : sizeof(item);
                assert(CMS_IncrBy(cms, item, len, 3, &count) == CMS_STATUS_OK);
                assert(CMS_Query(cms, item, len) == count);
                assert(CMS_IncrBy(cms, item, len, -1, &count) == CMS_STATUS_OK);
                for (size_t row = 0; row < cms->depth; ++row) {
                    /* Seed zero is the original MurmurHash2(item, len, row).
                     * Use wide arithmetic to independently verify 32-bit wrap. */
                    uint32_t rowSeed = (uint32_t)((uint64_t)seeds[s] + row);
                    reference[row * cms->width + MurmurHash2(item, len, rowSeed) % cms->width] += 2;
                }
            }
            assert(cms->counter == 200);
            for (size_t loc = 0; loc < 127 * 5; ++loc) {
                assert(getCell(cms, loc) == reference[loc]);
            }
            for (unsigned i = 0; i < 100; ++i) {
                char item[] = {(char)i, '\0', (char)0xff};
                size_t len = i == 0 ? 0 : sizeof(item);
                uint64_t expected = UINT64_MAX;
                for (size_t row = 0; row < cms->depth; ++row) {
                    uint32_t rowSeed = (uint32_t)((uint64_t)seeds[s] + row);
                    uint64_t cell = reference[row * cms->width +
                                              MurmurHash2(item, len, rowSeed) % cms->width];
                    if (cell < expected) expected = cell;
                }
                assert(CMS_Query(cms, item, len) == expected);
            }
            /* Force a late-row failure and verify rollback uses the same seed. */
            size_t last = 0;
            for (size_t row = 0; row < cms->depth; ++row) {
                uint32_t rowSeed = (uint32_t)((uint64_t)seeds[s] + row);
                last = row * cms->width + MurmurHash2("rollback", 8, rowSeed) % cms->width;
                setCell(cms, last, 9);
            }
            unsigned char before[127 * 5 * 8];
            for (int overflow = 0; overflow <= 1; ++overflow) {
                setCell(cms, last, overflow ? CMS_CELL_MAX(size) : 0);
                memcpy(before, cms->array, 127 * 5 * size);
                uint64_t count = 42;
                assert(CMS_IncrBy(cms, "rollback", 8, overflow ? 1 : -1, &count) ==
                       (overflow ? CMS_STATUS_OVERFLOW : CMS_STATUS_UNDERFLOW));
                assert(memcmp(before, cms->array, 127 * 5 * size) == 0);
                assert(cms->counter == 200 && count == 42);
            }
            CMS_Destroy(cms);
        }
    }
    return 0;
}
