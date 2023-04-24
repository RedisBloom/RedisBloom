/*
 * Copyright Redis Ltd. 2019 - present
 * Licensed under your choice of the Redis Source Available License 2.0 (RSALv2) or
 * the Server Side Public License v1 (SSPLv1).
 */

#pragma once

#include "redismodule.h"

#define DEFAULT_WIDTH 2.7
#define DEFAULT_DEPTH 5

#define CMS_ENC_VER 0

static inline bool _is_resp3(RedisModuleCtx *ctx) {
    int ctxFlags = RedisModule_GetContextFlags(ctx);
    if (ctxFlags & REDISMODULE_CTX_FLAGS_RESP3) {
        return true;
    }
    return false;
}

#define _ReplyMap(ctx) (RedisModule_ReplyWithMap != NULL && _is_resp3(ctx))

static void RedisModule_ReplyWithMapOrArray(RedisModuleCtx *ctx, long len, bool half) {
    if (_ReplyMap(ctx)) {
        RedisModule_ReplyWithMap(ctx, half ? len / 2 : len);
    } else {
        RedisModule_ReplyWithArray(ctx, len);
    }
}

int CMSModule_onLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc);
