#include "redismodule.h"
#include "test.h"
#include <rebloom.h>
#include <stdio.h>
#include <string.h>

static void *calloc_wrap(size_t a, size_t b) { return calloc(a, b); }
static void free_wrap(void *p) { free(p); }

static void testSbValidation() {
    sbChain *chain = sbCreateChain(1, 0.01);
    ASSERT_NE(chain, NULL);
    ASSERT_EQ(0, chain->total_entries);
    sbFreeChain(chain);

    ASSERT_EQ(NULL, sbCreateChain(0, 0.01));
    ASSERT_EQ(NULL, sbCreateChain(1, 0));
}

static void testSbBasic() {
    sbChain *chain = sbCreateChain(100, 0.01);
    ASSERT_NE(NULL, chain);

    const char *k1 = "hello";
    const size_t n1 = strlen(k1);

    ASSERT_EQ(0, sbCheck(chain, k1, n1));
    // Add the item once:
    ASSERT_NE(0, sbAdd(chain, k1, n1));
    ASSERT_EQ(1, chain->total_entries);
    ASSERT_NE(0, sbCheck(chain, k1, n1));

    sbFreeChain(chain);
}

static void testSbExpansion() {
    sbChain *chain = sbCreateChain(5, 0.01);
    ASSERT_NE(NULL, chain);

    // Add the first item
    ASSERT_NE(0, sbAdd(chain, "abc", 3));
    ASSERT_EQ(NULL, chain->cur->next);

    // Insert 5 items
    for (size_t ii = 0; ii < 5; ++ii) {
        ASSERT_EQ(0, sbCheck(chain, &ii, sizeof ii));
        ASSERT_NE(0, sbAdd(chain, &ii, sizeof ii));
    }
    ASSERT_NE(NULL, chain->cur->next);
    sbFreeChain(chain);
}

int main(int argc, char **argv) {
    RedisModule_Calloc = calloc_wrap;
    RedisModule_Free = free_wrap;
    testSbValidation();
    testSbBasic();
    testSbExpansion();
}