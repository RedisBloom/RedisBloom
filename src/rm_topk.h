#ifndef TOPK_MODULE_H
#define TOPK_MODULE_H

#include "redismodule.h"

int TopKModule_onLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc);

#endif