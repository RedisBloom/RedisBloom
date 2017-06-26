#include "redismodule.h"
#include "test.h"
#include <rebloom.h>
#include <stdio.h>
#include <string.h>

TEST_DEFINE_GLOBALS();

TEST_CLASS(basic);

static void *calloc_wrap(size_t a, size_t b) { return calloc(a, b); }
static void free_wrap(void *p) { free(p); }

TEST_F(basic, sbValidation) {
    SBChain *chain = SB_NewChain(1, 0.01);
    ASSERT_NE(chain, NULL);
    ASSERT_EQ(0, chain->size);
    SBChain_Free(chain);

    ASSERT_EQ(NULL, SB_NewChain(0, 0.01));
    ASSERT_EQ(NULL, SB_NewChain(1, 0));
}

TEST_F(basic, sbBasic) {
    SBChain *chain = SB_NewChain(100, 0.01);
    ASSERT_NE(NULL, chain);

    const char *k1 = "hello";
    const size_t n1 = strlen(k1);

    ASSERT_EQ(0, SBChain_Check(chain, k1, n1));
    // Add the item once:
    ASSERT_NE(0, SBChain_Add(chain, k1, n1));
    ASSERT_EQ(1, chain->size);
    ASSERT_NE(0, SBChain_Check(chain, k1, n1));

    SBChain_Free(chain);
}

TEST_F(basic, sbExpansion) {
    SBChain *chain = SB_NewChain(5, 0.01);
    ASSERT_NE(NULL, chain);

    // Add the first item
    ASSERT_NE(0, SBChain_Add(chain, "abc", 3));
    ASSERT_EQ(1, chain->nfitlers);

    // Insert 5 items
    for (size_t ii = 0; ii < 5; ++ii) {
        ASSERT_EQ(0, SBChain_Check(chain, &ii, sizeof ii));
        ASSERT_NE(0, SBChain_Add(chain, &ii, sizeof ii));
    }
    ASSERT_GT(chain->nfitlers, 1);
    SBChain_Free(chain);
}

int main(int argc, char **argv) {
    RedisModule_Calloc = calloc_wrap;
    RedisModule_Free = free_wrap;
    RedisModule_Realloc = realloc;
    TEST_RUN_ALL_TESTS();
    return 0;
}