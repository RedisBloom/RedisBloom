#ifndef APBF_MODULE_H
#define APBF_MODULE_H

#include "redismodule.h"

#define APBF_DEFAULT_LEVEL 4

#define APBF_ENC_VER 0

int APBFModule_onLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc);

#endif //APBF_MODULE_H