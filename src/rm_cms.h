
#pragma once

#include "redismodule.h"

#define DEFAULT_WIDTH 2.7
#define DEFAULT_DEPTH 5

#define CMS_ENC_VER 0

int CMSModule_onLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc);
