#ifndef TEST_H
#define TEST_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static int test__abort_on_fail = 0;

static void dump_int_value(const char *name, uint64_t value, FILE *out) {
    fprintf(out, "\t%s: U=%llu I=%lld H=0x%llx\n", name, (unsigned long long)value,
            (long long)value, (unsigned long long)value);
}

static inline void assert_int_helper(const char *var_a, const char *var_b, uint64_t val_a,
                                     uint64_t val_b, const char *oper, const char *file, int line) {
    fprintf(stderr, "Assertion failed at %s:%d\n", file, line);
    fprintf(stderr, "Expected: %s %s %s\n", var_a, oper, var_b);
    dump_int_value(var_a, val_a, stderr);
    dump_int_value(var_b, val_b, stderr);
    if (test__abort_on_fail) {
        abort();
    }
}

#define ASSERT_COMMON(a, b, oper)                                                                  \
    do {                                                                                           \
        uint64_t test__rv_a = (uint64_t)(a);                                                       \
        uint64_t test__rv_b = (uint64_t)(b);                                                       \
        if (!(test__rv_a oper test__rv_b)) {                                                       \
            assert_int_helper(#a, #b, test__rv_a, test__rv_b, #oper, __FILE__, __LINE__);          \
        }                                                                                          \
    } while (0)

#define ASSERT_EQ(a, b) ASSERT_COMMON(a, b, ==)
#define ASSERT_NE(a, b) ASSERT_COMMON(a, b, !=)
#define ASSERT_GT(a, b) ASSERT_COMMON(a, b, >)
#define ASSERT_GE(a, b) ASSERT_COMMON(a, b, >=)
#define ASSERT_LT(a, b) ASSERT_COMMON(a, b, <)
#define ASSERT_LE(a, b) ASSERT_COMMON(a, b, <=)

#endif