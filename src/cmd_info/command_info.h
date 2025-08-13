#pragma once

#include "redismodule.h"

/* Register documentation/metadata for Cuckoo Filter commands */
int RegisterCFCommandInfos(RedisModuleCtx *ctx);
int RegisterBFCommandInfos(RedisModuleCtx *ctx);
int RegisterCMSCommandInfos(RedisModuleCtx *ctx);
int RegisterTopKCommandInfos(RedisModuleCtx *ctx);

