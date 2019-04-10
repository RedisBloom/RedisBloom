#ifndef COUNTMINSKETCH_h
#define COUNTMINSKETCH_h

#include "./redismodule.h"

#define DEF_WIDTH 2000
#define DEF_DEPTH 5

typedef struct CMSketch {
  long long counter;
  int depth;
  int width;
  int *vector;
  unsigned int *hashA, *hashB;
} CMSketch;

/* TODO rid of *ctx in all functions. Move to rebloom.c */ 
int CMSInitCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc);
int CMSIncrByCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc);
int CMSQueryCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc);
int CMSMergeCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc);
int CMSDebugCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc);

#endif

