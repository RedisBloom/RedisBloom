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

int CMSModule_onLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc);
