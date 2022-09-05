
#pragma once

#include "redismodule.h"

#define TOPK_ENC_VER 0

int TopKModule_onLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc);
