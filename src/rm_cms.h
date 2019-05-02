#ifndef CMS_MODULE_H
#define CMS_MODULE_H

#include <redismodule.h>

#define DEFAULT_WIDTH 2000
#define DEFAULT_DEPTH 10

#define CMS_ENC_VER 0

int CMSModule_onLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc);

#endif