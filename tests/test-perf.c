#include "redismodule.h"
#include "sb.h"
#include <stdio.h>

#define NUM_ITERATIONS 50000000
#define NUM_ITEMS 100000
#define ERROR_RATE 0.0001

static void *calloc_wrap(size_t a, size_t b) { return calloc(a, b); }
static void free_wrap(void *p) { free(p); }

int main(int argc, char **argv) {
    RedisModule_Calloc = calloc_wrap;
    RedisModule_Free = free_wrap;
    RedisModule_Realloc = realloc;

    SBChain *chain = SB_NewChain(NUM_ITEMS, ERROR_RATE, 0);
    for (size_t ii = 0; ii < NUM_ITERATIONS; ++ii) {
        size_t elem = ii % NUM_ITEMS;
        SBChain_Add(chain, &elem, sizeof elem);
        SBChain_Check(chain, &elem, sizeof elem);
    }
    SBChain_Free(chain);
    return 0;
}