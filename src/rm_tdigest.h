
#pragma once

#include "redismodule.h"

#define TDIGEST_ENC_VER 0

int TDigestModule_onLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc);
