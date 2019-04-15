#ifndef CMS_H
#define CMS_H

#include "redismodule.h"

int CMSInit_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc);
int CMSIncrBy_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc);
int CMSQuery_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc);
int CMSMerge_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc);
int CMSDebug_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc);

#endif