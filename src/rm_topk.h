#ifndef TOPK_MODULE_H
#define TOPK_MODULE_H

#include "redismodule.h"

#define TOPK_ENC_VER 0
#define REDIS_MODULE_TARGET

int TopKModule_onLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc);

#endif