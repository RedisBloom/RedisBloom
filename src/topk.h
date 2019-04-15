#ifndef TOPK_H
#define TOPK_H

#include "redismodule.h"

int TopKAdd_RedisCommand(RedisModuleCtx *ctx, 
                         RedisModuleString **argv, int argc);
int TopKPRank_RedisCommand(RedisModuleCtx *ctx, 
                           RedisModuleString **argv, int argc);
int TopKPRange_RedisCommand(RedisModuleCtx *ctx, 
                            RedisModuleString **argv, int argc);
int TopKShrink_RedisCommand(RedisModuleCtx *ctx, 
                            RedisModuleString **argv, int argc);
int TopKDebug_RedisCommand(RedisModuleCtx *ctx, 
                           RedisModuleString **argv, int argc);
int TopKTest_RedisCommand(RedisModuleCtx *ctx, 
                          RedisModuleString **argv, int argc);

#endif