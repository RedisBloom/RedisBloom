#include "cuckoo.h"
#include "test.h"
#include "murmurhash2.h"
#include "redismodule.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

static void *calloc_wrap(size_t a, size_t b) { return calloc(a, b); }
static void free_wrap(void *p) { free(p); }

TEST_CLASS(cuckoo);
TEST_DEFINE_GLOBALS();

TEST_F(cuckoo, testBasicOps) {

    CuckooFilter ck;
    CuckooFilter_Init(&ck, 50);
    ASSERT_EQ(0, ck.numItems);
    ASSERT_EQ(1, ck.numFilters);
    ASSERT_EQ(16, ck.numBuckets);

    CuckooHash kfoo = CUCKOO_GEN_HASH("foo", 3);
    CuckooHash kbar = CUCKOO_GEN_HASH("bar", 3);
    CuckooHash kbaz = CUCKOO_GEN_HASH("baz", 3);

    ASSERT_EQ(CuckooInsert_Inserted, CuckooFilter_Insert(&ck, kfoo));
    ASSERT_EQ(CuckooInsert_Inserted, CuckooFilter_Insert(&ck, kbar));
    ASSERT_EQ(1, CuckooFilter_Check(&ck, kfoo));
    ASSERT_EQ(1, CuckooFilter_Check(&ck, kbar));
    ASSERT_NE(1, CuckooFilter_Check(&ck, kbaz));
    ASSERT_EQ(2, ck.numItems);

    ASSERT_EQ(CuckooInsert_Inserted, CuckooFilter_InsertUnique(&ck, kbaz));
    ASSERT_EQ(3, ck.numItems);

    ASSERT_EQ(CuckooInsert_Exists, CuckooFilter_InsertUnique(&ck, kfoo));

    // Try deleting items
    ASSERT_NE(0, CuckooFilter_Delete(&ck, kfoo));
    ASSERT_EQ(2, ck.numItems);
    ASSERT_EQ(0, CuckooFilter_Check(&ck, kfoo));

    ASSERT_EQ(0, CuckooFilter_Delete(&ck, kfoo));
    ASSERT_EQ(2, ck.numItems);

    CuckooFilter_Free(&ck);
}

TEST_F(cuckoo, testCount) {
    CuckooFilter ck;
    CuckooFilter_Init(&ck, 10);
    CuckooHash kfoo = CUCKOO_GEN_HASH("foo", 3);

    ASSERT_EQ(0, CuckooFilter_Count(&ck, kfoo));
    ASSERT_EQ(CuckooInsert_Inserted, CuckooFilter_Insert(&ck, kfoo));
    ASSERT_EQ(1, CuckooFilter_Count(&ck, kfoo));
    ASSERT_EQ(CuckooInsert_Inserted, CuckooFilter_Insert(&ck, kfoo));
    ASSERT_EQ(2, CuckooFilter_Count(&ck, kfoo));

    for (size_t ii = 0; ii < 8; ++ii) {
        ASSERT_EQ(CuckooInsert_Inserted, CuckooFilter_Insert(&ck, kfoo));
        ASSERT_EQ(3 + ii, CuckooFilter_Count(&ck, kfoo));
    }
    ASSERT_EQ(10, ck.numItems);
    CuckooFilter_Free(&ck);
}

#define NUM_BULK 10000

TEST_F(cuckoo, testRelocations) {
    test__abort_on_fail = 1;
    CuckooFilter ck;
    CuckooFilter_Init(&ck, 50);
    ASSERT_EQ(0, ck.numItems);
    ASSERT_EQ(1, ck.numFilters);
    ASSERT_EQ(16, ck.numBuckets);

    for (size_t ii = 0; ii < NUM_BULK; ++ii) {
        CuckooHash hash = CUCKOO_GEN_HASH(&ii, sizeof ii);
        ASSERT_EQ(CuckooInsert_Inserted, CuckooFilter_Insert(&ck, hash));
        ASSERT_NE(0, CuckooFilter_Check(&ck, hash));
    }

    for (size_t ii = 0; ii < NUM_BULK; ++ii) {
        CuckooHash hash = CUCKOO_GEN_HASH(&ii, sizeof ii);
        ASSERT_NE(0, CuckooFilter_Check(&ck, hash));
    }

    for (size_t ii = 0; ii < NUM_BULK; ++ii) {
        CuckooHash hash = CUCKOO_GEN_HASH(&ii, sizeof ii);
        ASSERT_EQ(CuckooInsert_Exists, CuckooFilter_InsertUnique(&ck, hash));
        ASSERT_EQ(CuckooInsert_Inserted, CuckooFilter_Insert(&ck, hash));
    }

    CuckooFilter_Free(&ck);
}

int main(int argc, char **argv) {
    test__abort_on_fail = 1;
    RedisModule_Calloc = calloc_wrap;
    RedisModule_Free = free_wrap;
    RedisModule_Realloc = realloc;
    RedisModule_Alloc = malloc;

    TEST_RUN_ALL_TESTS();
    return 0;
}