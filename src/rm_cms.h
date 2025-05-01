/*
 * Copyright (c) 2006-Present, Redis Ltd.
 * All rights reserved.
 *
 * Licensed under your choice of (a) the Redis Source Available License 2.0
 * (RSALv2); or (b) the Server Side Public License v1 (SSPLv1); or (c) the
 * GNU Affero General Public License v3 (AGPLv3).
 */

#pragma once

#include "redismodule.h"
#include <stdbool.h>

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
