#ifndef TDIGEST_MODULE_H
#define TDIGEST_MODULE_H
#define REDISMODULE_MAIN
#include "redismodule.h"

#define TDIGEST_ENC_VER 0
#define REDIS_MODULE_TARGET

int TDigestModule_onLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc);

#endif
