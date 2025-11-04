#ifndef REDIS_BLOOM_LOAD_IO_ERROR_MACROS_H
#define REDIS_BLOOM_LOAD_IO_ERROR_MACROS_H

#include "common.h"

///
/// `defer` and `errdefer` are macros that are used to execute a block of code at the end of the
/// current scope. They behave similarly to the `defer` and `errdefer` keywords in languages like
/// Go, Swift, and Zig.
///
#if defined(__GNUC__) && !defined(__clang__)
#define DEFER_(F, V)                                                                               \
    auto inline __attribute__((__always_inline__)) void F(int *);                                  \
    int V __attribute__((__cleanup__(F)));                                                         \
    void F(__attribute__((__unused__)) int *_)
#define DEFERER_(L, C) DEFER_(DEFER_FUNC_##L##C, DEFER_VAR_##L##C)
#elif defined(__clang__)
#include <Block.h>
static inline __attribute__((__always_inline__)) void defer_cleanup_(void (^*block)(void)) {
    (*block)();
}
#define DEFER_(B) __attribute__((__cleanup__(defer_cleanup_))) void (^B)(void) = ^
#define DEFERER_(L, C) DEFER_(DEFER_BLOCK_##L##C)
#else
#error "defer is not supported on this compiler"
#endif

#define DEFERER(L, C) DEFERER_(L, C)

/// defer is used to execute code unconditionally at the end of the current scope.
/// usage: `defer compound-statement`
/// example:
/// ```
/// FILE *f = fopen(some_file, "r");
/// defer { fclose(f); }
/// ```
#define defer DEFERER(__LINE__, __COUNTER__)

/// errdefer is used to execute code at the end of the current scope only if an error occurred.
/// usage: `errdefer(err, compound-statement)`
/// example:
/// ```
/// bool err = false;
/// void *p = malloc(10);
/// errdefer(err, { free(p); })
/// ```
#define ERRDEFER_CONCAT_(a, b) a##b
#define ERRDEFER_CONCAT(a, b) ERRDEFER_CONCAT_(a, b)

#define ERRDEFER_IMPL_(err, err_ptr_name, ...)                                                     \
    typeof(err) *err_ptr_name = &(err);                                                            \
    defer {                                                                                        \
        if (unlikely(*err_ptr_name))                                                               \
            __VA_ARGS__;                                                                           \
    }

#define ERRDEFER_IMPL(err, counter, ...)                                                           \
    ERRDEFER_IMPL_(err, ERRDEFER_CONCAT(ERRDEFER_PTR_, counter), __VA_ARGS__)

#define errdefer(err, ...)                                                                         \
    ERRDEFER_IMPL(err, __COUNTER__, __VA_ARGS__)

#define LoadDouble_IOError(rdb, is_err, ret)                                                       \
    __extension__({                                                                                \
        double res = RedisModule_LoadDouble((rdb));                                                \
        if (RedisModule_IsIOError(rdb)) {                                                          \
            is_err = true;                                                                         \
            return ret;                                                                            \
        }                                                                                          \
        res;                                                                                       \
    })

#define LoadUnsigned_IOError(rdb, is_err, ret)                                                     \
    __extension__({                                                                                \
        uint64_t res = RedisModule_LoadUnsigned((rdb));                                            \
        if (RedisModule_IsIOError(rdb)) {                                                          \
            is_err = true;                                                                         \
            return ret;                                                                            \
        }                                                                                          \
        res;                                                                                       \
    })

#define LoadSigned_IOError(rdb, is_err, ret)                                                       \
    __extension__({                                                                                \
        int64_t res = RedisModule_LoadSigned((rdb));                                               \
        if (RedisModule_IsIOError(rdb)) {                                                          \
            is_err = true;                                                                         \
            return ret;                                                                            \
        }                                                                                          \
        res;                                                                                       \
    })

#define LoadString_IOError(rdb, is_err, ret)                                                       \
    __extension__({                                                                                \
        RedisModuleString *res = RedisModule_LoadString((rdb));                                    \
        if (RedisModule_IsIOError(rdb)) {                                                          \
            is_err = true;                                                                         \
            return ret;                                                                            \
        }                                                                                          \
        res;                                                                                       \
    })

#define LoadStringBuffer_IOError(rdb, len, is_err, ret)                                            \
    __extension__({                                                                                \
        char *res = RedisModule_LoadStringBuffer((rdb), (len));                                    \
        if (RedisModule_IsIOError(rdb)) {                                                          \
            is_err = true;                                                                         \
            return ret;                                                                            \
        }                                                                                          \
        res;                                                                                       \
    })

#endif