/*
 * Copyright Redis Ltd. 2017 - present
 * Licensed under your choice of the Redis Source Available License 2.0 (RSALv2) or
 * the Server Side Public License v1 (SSPLv1).
 */

#pragma once

#include "redismodule.h"
#if defined(DEBUG) || !defined(NDEBUG)
#include "readies/cetara/diag/gdb.h"
#endif

static inline void *defragPtr(RedisModuleDefragCtx *ctx, void *ptr) {
    void *tmp = RedisModule_DefragAlloc(ctx, ptr);
    return tmp ? tmp : ptr;
}
