/**
 * Adaptive histogram based on something like streaming k-means crossed with Q-digest.
 * The implementation is a direct descendent of MergingDigest
 * https://github.com/tdunning/t-digest/
 *
 * Copyright (c) 2021 Redis, All rights reserved.
 *
 * Allocator selection.
 *
 * This file is used in order to change the t-digest allocator at compile time.
 * Just define the following defines to what you want to use. Also add
 * the include of your alternate allocator if needed (not needed in order
 * to use the default libc allocator). */

#ifndef TD_ALLOC_H
#define TD_ALLOC_H
#include "../deps/RedisModulesSDK/redismodule.h"
#define td_malloc_(...)                                                                            \
    RedisModule_TryAlloc ? RedisModule_TryAlloc(__VA_ARGS__) : RedisModule_Alloc(__VA_ARGS__)
#define td_calloc_(...)                                                                            \
    RedisModule_TryCalloc ? RedisModule_TryCalloc(__VA_ARGS__) : RedisModule_Calloc(__VA_ARGS__)
#define td_realloc_(...)                                                                           \
    RedisModule_TryRealloc ? RedisModule_TryRealloc(__VA_ARGS__) : RedisModule_Realloc(__VA_ARGS__)
#define td_free_ RedisModule_Free
#endif
