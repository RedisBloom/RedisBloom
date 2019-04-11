#ifndef CMS_H
#define CMS_H

#include "redismodule.h"

static int CMSInit_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc);
static int CMSIncrBy_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc);
int CMSQueryCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc);


#endif