#include <math.h>    // ceil, log10f
#include <stdlib.h>  // malloc
#include <strings.h> // strncasecmp

#include "rmutil/util.h"
#include "version.h"

#include "cms.h"
#include "rm_cms.h"

#define INNER_ERROR(x)                                                                             \
    RedisModule_ReplyWithError(ctx, x);                                                            \
    return REDISMODULE_ERR;

RedisModuleType *CMSketchType;

typedef struct {
    const char *key;
    size_t keylen;
    long long value;
} CMSPair;

static int GetCMSKey(RedisModuleCtx *ctx, RedisModuleString *keyName, CMSketch **cms, int mode) {
    // All using this function should call RedisModule_AutoMemory to prevent memory leak
    RedisModuleKey *key = RedisModule_OpenKey(ctx, keyName, mode);
    if (RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY) {
        INNER_ERROR("CMS: key does not exist");
    } else if (RedisModule_ModuleTypeGetType(key) != CMSketchType) {
        INNER_ERROR(REDISMODULE_ERRORMSG_WRONGTYPE);
    }
    *cms = RedisModule_ModuleTypeGetValue(key);
    return REDISMODULE_OK;
}

static int parseCreateArgs(RedisModuleCtx *ctx, RedisModuleString **argv, int argc,
                           long long *width, long long *depth) {

    size_t cmdlen;
    const char *cmd = RedisModule_StringPtrLen(argv[0], &cmdlen);
    if (strcasecmp(cmd, "cms.initbydim") == 0) {
        if ((RedisModule_StringToLongLong(argv[2], width) != REDISMODULE_OK) || *width < 1) {
            INNER_ERROR("CMS: invalid width");
        }
        if ((RedisModule_StringToLongLong(argv[3], depth) != REDISMODULE_OK) || *depth < 1) {
            INNER_ERROR("CMS: invalid depth");
        }
    } else {
        double overEst = 0, prob = 0;
        if ((RedisModule_StringToDouble(argv[2], &overEst) != REDISMODULE_OK) || overEst <= 0 ||
            overEst >= 1) {
            INNER_ERROR("CMS: invalid overestimation value");
        }
        if ((RedisModule_StringToDouble(argv[3], &prob) != REDISMODULE_OK) ||
            prob <= 0 || prob >= 1) {
            INNER_ERROR("CMS: invalid prob value");
        }
        CMS_DimFromProb(overEst, prob, (size_t *)width, (size_t *)depth);
    }

    return REDISMODULE_OK;
}

int CMSketch_Create(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    if (argc != 4) {
        return RedisModule_WrongArity(ctx);
    }

    CMSketch *cms = NULL;
    long long width = 0, depth = 0;
    RedisModuleString *keyName = argv[1];
    RedisModuleKey *key = RedisModule_OpenKey(ctx, keyName, REDISMODULE_READ | REDISMODULE_WRITE);

    if (RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_EMPTY) {
        RedisModule_CloseKey(key);
        return RedisModule_ReplyWithError(ctx, "CMS: key already exists");
    }

    if (parseCreateArgs(ctx, argv, argc, &width, &depth) != REDISMODULE_OK)
        return REDISMODULE_OK;

    cms = NewCMSketch(width, depth);
    RedisModule_ModuleTypeSetValue(key, CMSketchType, cms);

    RedisModule_CloseKey(key);
    RedisModule_ReplicateVerbatim(ctx);
    RedisModule_ReplyWithSimpleString(ctx, "OK");
    return REDISMODULE_OK;
}

static int parseIncrByArgs(RedisModuleCtx *ctx, RedisModuleString **argv, int argc, CMSPair **pairs,
                           int qty) {
    for (int i = 0; i < qty; ++i) {
        (*pairs)[i].key = RedisModule_StringPtrLen(argv[2 + i * 2], &(*pairs)[i].keylen);
        RedisModule_StringToLongLong(argv[2 + i * 2 + 1], &((*pairs)[i].value));
    }
    return REDISMODULE_OK;
}

int CMSketch_IncrBy(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc < 4 || (argc % 2) == 1) {
        return RedisModule_WrongArity(ctx);
    }

    RedisModuleString *keyName = argv[1];
    RedisModuleKey *key = RedisModule_OpenKey(ctx, keyName, REDISMODULE_READ);
    CMSketch *cms = NULL;

    if (RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY) {
        INNER_ERROR("CMS: key does not exist");
    } else if (RedisModule_ModuleTypeGetType(key) != CMSketchType) {
        return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    } else {
        cms = RedisModule_ModuleTypeGetValue(key);
    }

    int pairCount = (argc - 2) / 2;
    CMSPair *pairArray = CMS_CALLOC(pairCount, sizeof(CMSPair));
    parseIncrByArgs(ctx, argv, argc, &pairArray, pairCount);
    RedisModule_ReplyWithArray(ctx, pairCount);
    for (int i = 0; i < pairCount; ++i) {
        size_t count = CMS_IncrBy(cms, pairArray[i].key, pairArray[i].keylen, pairArray[i].value);
        RedisModule_ReplyWithLongLong(ctx, (long long)count);
    }

    CMS_FREE(pairArray);
    RedisModule_CloseKey(key);
    RedisModule_ReplicateVerbatim(ctx);
    return REDISMODULE_OK;
}

int CMSketch_Query(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    if (argc < 3) {
        return RedisModule_WrongArity(ctx);
    }

    CMSketch *cms = NULL;
    if (GetCMSKey(ctx, argv[1], &cms, REDISMODULE_READ) != REDISMODULE_OK) {
        return REDISMODULE_OK;
    }

    int itemCount = argc - 2;
    size_t length = 0;
    RedisModule_ReplyWithArray(ctx, itemCount);
    for (int i = 0; i < itemCount; ++i) {
        const char *str = RedisModule_StringPtrLen(argv[2 + i], &length);
        RedisModule_ReplyWithLongLong(ctx, CMS_Query(cms, str, length));
    }

    return REDISMODULE_OK;
}

static int parseMergeArgs(RedisModuleCtx *ctx, RedisModuleString **argv, int argc,
                          mergeParams *params) {
    long long numKeys = params->numKeys;
    int pos = RMUtil_ArgIndex("WEIGHTS", argv, argc);
    if (pos < 0) {
        if (numKeys != argc - 3) {
            INNER_ERROR("CMS: wrong number of keys");
        }
    } else {
        if ((pos != 3 + numKeys) && (argc != 4 + numKeys * 2)) {
            INNER_ERROR("CMS: wrong number of keys/weights");
        }
    }

    if (GetCMSKey(ctx, argv[1], &(params->dest), REDISMODULE_READ | REDISMODULE_WRITE) !=
        REDISMODULE_OK) {
        return REDISMODULE_ERR;
    }

    size_t width = params->dest->width;
    size_t depth = params->dest->depth;

    for (int i = 0; i < numKeys; ++i) {
        if (pos == -1) {
            params->weights[i] = 1;
        } else if (RedisModule_StringToLongLong(argv[3 + numKeys + 1 + i], &(params->weights[i])) !=
                   REDISMODULE_OK) {
            INNER_ERROR("CMS: invalid weight value");
        }
        if (GetCMSKey(ctx, argv[3 + i], &(params->cmsArray[i]), REDISMODULE_READ) !=
            REDISMODULE_OK) {
            return REDISMODULE_ERR;
        }
        if (params->cmsArray[i]->width != width || params->cmsArray[i]->depth != depth) {
            INNER_ERROR("CMS: width/depth is not equal");
        }
    }

    return REDISMODULE_OK;
}

int CMSketch_Merge(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    if (argc < 4) {
        return RedisModule_WrongArity(ctx);
    }

    mergeParams params = {0};
    if (RedisModule_StringToLongLong(argv[2], &(params.numKeys)) != REDISMODULE_OK) {
        return RedisModule_ReplyWithError(ctx, "CMS: invalid numkeys");
    }

    params.cmsArray = CMS_CALLOC(params.numKeys, sizeof(CMSketch *));
    params.weights = CMS_CALLOC(params.numKeys, sizeof(long long));

    if (parseMergeArgs(ctx, argv, argc, &params) != REDISMODULE_OK) {
        CMS_FREE(params.cmsArray);
        CMS_FREE(params.weights);
        return REDISMODULE_OK;
    }

    CMS_MergeParams(params);

    CMS_FREE(params.cmsArray);
    CMS_FREE(params.weights);
    RedisModule_ReplicateVerbatim(ctx);
    RedisModule_ReplyWithSimpleString(ctx, "OK");
    return REDISMODULE_OK;
}

int CMSKetch_Info(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    if (argc != 2)
        return RedisModule_WrongArity(ctx);

    CMSketch *cms = NULL;
    if (GetCMSKey(ctx, argv[1], &cms, REDISMODULE_READ) != REDISMODULE_OK) {
        return REDISMODULE_OK;
    }

    RedisModule_ReplyWithArray(ctx, 3 * 2);
    RedisModule_ReplyWithSimpleString(ctx, "width");
    RedisModule_ReplyWithLongLong(ctx, cms->width);
    RedisModule_ReplyWithSimpleString(ctx, "depth");
    RedisModule_ReplyWithLongLong(ctx, cms->depth);
    RedisModule_ReplyWithSimpleString(ctx, "count");
    RedisModule_ReplyWithLongLong(ctx, cms->counter);

    return REDISMODULE_OK;
}

void CMSRdbSave(RedisModuleIO *io, void *obj) {
    CMSketch *cms = obj;
    RedisModule_SaveUnsigned(io, cms->width);
    RedisModule_SaveUnsigned(io, cms->depth);
    RedisModule_SaveUnsigned(io, cms->counter);
    RedisModule_SaveStringBuffer(io, (const char *)cms->array,
                                 cms->width * cms->depth * sizeof(uint32_t));
}

void *CMSRdbLoad(RedisModuleIO *io, int encver) {
    if (encver > CMS_ENC_VER) {
        return NULL;
    }

    CMSketch *cms = CMS_CALLOC(1, sizeof(CMSketch));
    cms->width = RedisModule_LoadUnsigned(io);
    cms->depth = RedisModule_LoadUnsigned(io);
    cms->counter = RedisModule_LoadUnsigned(io);
    size_t length = cms->width * cms->depth * sizeof(size_t);
    cms->array = (uint32_t *)RedisModule_LoadStringBuffer(io, &length);

    return cms;
}

void CMSFree(void *value) { CMS_Destroy(value); }

size_t CMSMemUsage(const void *value) {
    CMSketch *cms = (CMSketch *)value;
    return sizeof(cms) + cms->width * cms->depth * sizeof(size_t);
}

int CMSModule_onLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    // TODO: add option to set defaults from command line and in program
    RedisModuleTypeMethods tm = {.version = REDISMODULE_TYPE_METHOD_VERSION,
                                 .rdb_load = CMSRdbLoad,
                                 .rdb_save = CMSRdbSave,
                                 .aof_rewrite = RMUtil_DefaultAofRewrite,
                                 .mem_usage = CMSMemUsage,
                                 .free = CMSFree};

    CMSketchType = RedisModule_CreateDataType(ctx, "CMSk-TYPE", CMS_ENC_VER, &tm);
    if (CMSketchType == NULL)
        return REDISMODULE_ERR;

    RMUtil_RegisterWriteDenyOOMCmd(ctx, "cms.initbydim", CMSketch_Create);
    RMUtil_RegisterWriteDenyOOMCmd(ctx, "cms.initbyprob", CMSketch_Create);
    RMUtil_RegisterWriteDenyOOMCmd(ctx, "cms.incrby", CMSketch_IncrBy);
    RMUtil_RegisterReadCmd(ctx, "cms.query", CMSketch_Query);
    RMUtil_RegisterWriteDenyOOMCmd(ctx, "cms.merge", CMSketch_Merge);
    RMUtil_RegisterReadCmd(ctx, "cms.info", CMSKetch_Info);

    return REDISMODULE_OK;
}