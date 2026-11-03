/* Run from the repository root: gmake unit-tests */
/* Keep checks active in release unit-test builds too. */
#undef NDEBUG
#include <assert.h>
#include <string.h>
#include "seed.h"

static uint64_t random_value;
static unsigned random_calls;

static void mockRandomBytes(unsigned char *dst, size_t len) {
    assert(len == sizeof(random_value));
    memcpy(dst, &random_value, len);
    ++random_calls;
}

int main(void) {
    RedisModule_GetRandomBytes = mockRandomBytes;
    uint64_t value = 42;
    assert(Seed_Parse("0", 1, &value) == SEED_INPUT_MANUAL && value == 0);
    assert(Seed_Parse("123", 3, &value) == SEED_INPUT_MANUAL && value == 123);
    assert(Seed_Parse("0123", 4, &value) == SEED_INPUT_MANUAL && value == 123);
    assert(Seed_Parse("08", 2, &value) == SEED_INPUT_MANUAL && value == 8);
    assert(Seed_Parse("0x0", 3, &value) == SEED_INPUT_MANUAL && value == 0);
    assert(Seed_Parse("0xaBcDeF", 8, &value) == SEED_INPUT_MANUAL && value == 0xabcdef);
    assert(Seed_Parse("0XFFFFFFFFFFFFFFFF", 18, &value) == SEED_INPUT_MANUAL &&
           value == UINT64_MAX);
    assert(Seed_Parse("18446744073709551615", 20, &value) == SEED_INPUT_MANUAL &&
           value == UINT64_MAX);
    assert(Seed_Parse("0000000000000000", 16, &value) == SEED_INPUT_MANUAL && value == 0);
    const char raw[] = {'1', '7', '1'};
    assert(Seed_Parse(raw, sizeof(raw), &value) == SEED_INPUT_MANUAL && value == 0xab);

    const char *random_inputs[] = {"random", "RANDOM", "RaNdOm"};
    for (size_t i = 0; i < sizeof(random_inputs) / sizeof(*random_inputs); ++i) {
        assert(Seed_Parse(random_inputs[i], strlen(random_inputs[i]), &value) == SEED_INPUT_RANDOM);
        assert(value == 0xab);
    }
    const char *invalid[] = {"", "0x", "0xg", "abc", "+1", "-1", " 1", "1 ", "a b",
                             "g", "randomx", "18446744073709551616", "0x10000000000000000",
                             "000000000000000000000", "1\n", "0b10", "0x-1", "0x 1",
                             "0x+1", "\t1", "1.0", "1e3", "0x1p2", "rand", "random "};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i) {
        assert(Seed_Parse(invalid[i], strlen(invalid[i]), &value) == SEED_INPUT_INVALID);
        assert(value == 0xab);
    }
    assert(Seed_Parse("a\0b", 3, &value) == SEED_INPUT_INVALID && value == 0xab);
    assert(Seed_Parse("1\0", 2, &value) == SEED_INPUT_INVALID && value == 0xab);
    assert(Seed_Parse("ran\0om", 6, &value) == SEED_INPUT_INVALID && value == 0xab);
    assert(Seed_Parse(NULL, 1, &value) == SEED_INPUT_INVALID);
    assert(Seed_Parse("a", 1, NULL) == SEED_INPUT_INVALID);
    assert(random_calls == 0); /* Parsing does not generate a seed. */

    random_value = UINT64_C(0xfedcba9876543210);
    assert(Seed_Generate() == random_value);
    assert(random_calls == 1);
    random_value = 0; /* Zero is valid, even when randomly generated. */
    assert(Seed_Generate() == 0);
    assert(random_calls == 2);
    return 0;
}
