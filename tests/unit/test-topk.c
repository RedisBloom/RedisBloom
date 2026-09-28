/* Run with gmake unit-tests. Keep assertions active in release builds. */
#undef NDEBUG
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "redismodule.h"
#include "topk.h"
#include "murmur2/murmurhash2.h"

int main(void) {
    RedisModule_Calloc = calloc;
    RedisModule_Free = free;
    /* The final seed makes the empty item's fingerprint zero. */
    const uint32_t seeds[] = {0, 123, UINT32_C(0x80000000), UINT32_MAX,
                              UINT32_MAX - 1918};
    const char *items[] = {"", "page:123", "a\0b\xff"};
    const size_t lengths[] = {0, 8, 4};
    for (size_t s = 0; s < sizeof(seeds) / sizeof(*seeds); ++s) {
        for (size_t n = 0; n < sizeof(items) / sizeof(*items); ++n) {
            TopK *topk = TopK_Create(1, 127, 5, 0.9);
            assert(topk && topk->seed == 0);
            topk->seed = seeds[s];
            assert(!TopK_Query(topk, items[n], lengths[n]));
            assert(TopK_Add(topk, items[n], lengths[n], 3) == NULL);
            assert(TopK_Add(topk, items[n], lengths[n], 4) == NULL);
            assert(TopK_Query(topk, items[n], lengths[n]));
            assert(TopK_Count(topk, items[n], lengths[n]) == 7);
            /* Wide arithmetic independently checks 32-bit wraparound. Seed zero
             * reproduces the original row-number hashes and fingerprint 1919. */
            uint32_t fpSeed = (uint32_t)((uint64_t)seeds[s] + 1919);
            uint32_t fp = MurmurHash2(items[n], lengths[n], fpSeed);
            for (uint32_t row = 0; row < topk->depth; ++row) {
                uint32_t rowSeed = (uint32_t)((uint64_t)seeds[s] + row);
                uint32_t column = MurmurHash2(items[n], lengths[n], rowSeed) % topk->width;
                for (uint32_t col = 0; col < topk->width; ++col) {
                    Bucket *bucket = &topk->data[row * topk->width + col];
                    assert(bucket->count == (col == column ? 7 : 0));
                    assert(bucket->fp == (col == column ? fp : 0));
                }
            }
            HeapBucket *list = TopK_List(topk);
            assert(list[0].fp == fp && list[0].count == 7);
            assert(list[0].item != NULL);
            assert(list[0].itemlen == lengths[n]);
            assert(memcmp(list[0].item, items[n], lengths[n]) == 0);
            free(list);
            TopK_Destroy(topk);
        }
        /* Force collisions and heap replacement with the same per-key seed. */
        TopK *topk = TopK_Create(1, 1, 3, 1.0);
        topk->seed = seeds[s];
        srand(1);
        assert(TopK_Add(topk, "old", 3, 1) == NULL);
        char *expelled = TopK_Add(topk, "new", 3, 10);
        assert(expelled && strcmp(expelled, "old") == 0);
        free(expelled);
        assert(!TopK_Query(topk, "old", 3));
        assert(TopK_Query(topk, "new", 3));
        assert(TopK_Count(topk, "new", 3) == 10);
        TopK_Destroy(topk);
    }
    return 0;
}
