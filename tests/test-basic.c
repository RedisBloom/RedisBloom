#include "redismodule.h"
#include "sb.h"
#include "test.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <math.h>

TEST_DEFINE_GLOBALS();

TEST_CLASS(basic);

static void *calloc_wrap(size_t a, size_t b) { return calloc(a, b); }
static void free_wrap(void *p) { free(p); }

TEST_F(basic, sbValidation) {
    SBChain *chain = SB_NewChain(1, 0.01, 0);
    ASSERT_NE(chain, NULL);
    ASSERT_EQ(0, chain->size);
    SBChain_Free(chain);

    ASSERT_EQ(NULL, SB_NewChain(0, 0.01, 0));
    ASSERT_EQ(NULL, SB_NewChain(1, 0, 0));
}

TEST_F(basic, sbBasic) {
    SBChain *chain = SB_NewChain(100, 0.01, 0);
    ASSERT_NE(NULL, chain);

    const char *k1 = "hello";
    const size_t n1 = strlen(k1);

    ASSERT_EQ(0, SBChain_Check(chain, k1, n1));
    // Add the item once:
    ASSERT_NE(0, SBChain_Add(chain, k1, n1));
    ASSERT_EQ(1, chain->size);
    ASSERT_NE(0, SBChain_Check(chain, k1, n1));
    // Add the item again:
    ASSERT_EQ(0, SBChain_Add(chain, k1, n1));

    SBChain_Free(chain);
}

TEST_F(basic, sbExpansion) {
    // Note that the chain auto-expands to 6 items by default with the given
    // error ratio. If you modify the error ratio, the expansion may change.
    SBChain *chain = SB_NewChain(6, 0.01, 0);
    ASSERT_NE(NULL, chain);

    // Add the first item
    ASSERT_NE(0, SBChain_Add(chain, "abc", 3));
    ASSERT_EQ(1, chain->nfilters);

    // Insert 6 items
    for (size_t ii = 0; ii < 6; ++ii) {
        ASSERT_EQ(0, SBChain_Check(chain, &ii, sizeof ii));
        ASSERT_NE(0, SBChain_Add(chain, &ii, sizeof ii));
    }
    ASSERT_GT(chain->nfilters, 1);
    SBChain_Free(chain);
}

TEST_F(basic, testIssue6_Overflow) {
    SBChain *chain = SB_NewChain(1000000000000, 0.00001, 0);
    if (chain != NULL) {
        SBChain_Free(chain);
    } else {
        ASSERT_EQ(ENOMEM, errno);
    }

    chain = SB_NewChain(4294967296, 0.00001, 0);
    ASSERT_EQ(NULL, chain);
}

TEST_F(basic, testIssue7_Overflow) {
    // Try with a bit count of 33:
    SBChain *chain = SB_NewChain(33, 0.000025, BLOOM_OPT_ENTS_IS_BITS);
    if (chain == NULL) {
        ASSERT_EQ(ENOMEM, errno);
        return;
    }

    ASSERT_NE(0, SBChain_Add(chain, "foo", 3));
    ASSERT_NE(0, SBChain_Add(chain, "bar", 3));
    ASSERT_EQ(2, chain->size);
    ASSERT_EQ(2, chain->filters[0].size);

    struct bloom *inner = &chain->filters[0].inner;
    ASSERT_EQ(33, inner->n2);
    ASSERT_EQ(0.000025, inner->error);
    ASSERT_EQ(1073741824, inner->bytes);
    ASSERT_EQ(389468927, inner->entries);

    SBChain_Free(chain);
}

TEST_F(basic, testIssue9) {
    SBChain *chain = SB_NewChain(350000000, 0.01, 0);
    if (chain == NULL) {
        ASSERT_EQ(ENOMEM, errno);
        return;
    }

    ASSERT_NE(0, SBChain_Add(chain, "asdf", 4));
    ASSERT_NE(0, SBChain_Add(chain, "a", 1));
    ASSERT_NE(0, SBChain_Add(chain, "s", 1));
    ASSERT_NE(0, SBChain_Add(chain, "d", 1));
    ASSERT_NE(0, SBChain_Add(chain, "f", 1));

    SBChain_Free(chain);
}

int main(int argc, char **argv) {
    RedisModule_Calloc = calloc_wrap;
    RedisModule_Free = free_wrap;
    RedisModule_Realloc = realloc;
    TEST_RUN_ALL_TESTS();
    return 0;
}