#include <strings.h> // strncasecmp
#include <stdlib.h>  // malloc
#include <math.h>    // ceil, log10f

#include "version.h"
#include "rmutil/util.h"

#include "rm_cms.h"
#include "cms.h"

#define INNER_ERROR(x)                                                                             \
    RedisModule_ReplyWithError(ctx, x);                                                            \
    return REDISMODULE_ERR;

RedisModuleType *CMSketchType;

typedef struct {
    const char *key;
    size_t keylen;
    long long value;
} CMSPair;

static int cmpCStrToRedisStr(const char *str, RedisModuleString *redisStr) {
    size_t len;
    char *cRedisStr = (char *)RedisModule_StringPtrLen(redisStr, &len);
    return strncasecmp(cRedisStr, str, len) == 0;
}

static int GetCMSKey(RedisModuleCtx *ctx, RedisModuleString *keyName, CMSketch **cms, int mode) {
    RedisModuleKey *key = RedisModule_OpenKey(ctx, keyName, mode);
    if (RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY) {
        INNER_ERROR("CMS: key does not exist");
    } else if (RedisModule_ModuleTypeGetType(key) != CMSketchType) {
        INNER_ERROR(REDISMODULE_ERRORMSG_WRONGTYPE);
    }
    *cms = RedisModule_ModuleTypeGetValue(key);
    return REDISMODULE_OK;
}

static int CreateCMSKey(RedisModuleCtx *ctx, RedisModuleString *keyName, long long width,
                        long long depth, CMSketch **cms, RedisModuleKey **key) {
    if (*key == NULL) {
        *key = RedisModule_OpenKey(ctx, keyName, REDISMODULE_READ | REDISMODULE_WRITE);
    }

    *cms = NewCMSketch(width, depth);

    if (RedisModule_ModuleTypeSetValue(*key, CMSketchType, *cms) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}

static int parseCreateArgs(RedisModuleCtx *ctx, RedisModuleString **argv, int argc,
                           long long *width, long long *depth) {
    if (argc == 4) {
        if ((RedisModule_StringToLongLong(argv[2], width) != REDISMODULE_OK) || *width < 1) {
            INNER_ERROR("CMS: invalid width");
        }
        if ((RedisModule_StringToLongLong(argv[3], depth) != REDISMODULE_OK) || *depth < 1) {
            INNER_ERROR("CMS: invalid depth");
        }
    } else {
        long long n = 0;
        double overEst = 0, prob = 0;
        if ((RedisModule_StringToLongLong(argv[2], &n) != REDISMODULE_OK) || (n < 1)) {
            INNER_ERROR("CMS: invalid n value");
        }
        if ((RedisModule_StringToDouble(argv[3], &overEst) != REDISMODULE_OK) || overEst <= 0 ||
            overEst >= 1) {
            INNER_ERROR("CMS: invalid overestimation value");
        }
        if ((RedisModule_StringToDouble(argv[4], &prob) != REDISMODULE_OK) ||
            (prob <= 0 || prob >= 1)) {
            INNER_ERROR("CMS: invalid prob value");
        }
        CMS_DimFromProb(n, overEst, prob, (size_t *)width, (size_t *)depth);
    }

    return REDISMODULE_OK;
}

int CMSketch_Create(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    if ((argc == 4 && cmpCStrToRedisStr("cms.initbydim", argv[0])) == 0 &&
        (argc == 5 && cmpCStrToRedisStr("cms.initbyprob", argv[0])) == 0) {
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

    CreateCMSKey(ctx, keyName, width, depth, &cms, &key);

    RedisModule_CloseKey(key);
    RedisModule_ReplyWithSimpleString(ctx, "OK");
    RedisModule_ReplicateVerbatim(ctx);
    return REDISMODULE_OK;
}
/*************************************************/
/*          After change of API                  */
/*************************************************/
static int parseCreateArgsNew(RedisModuleCtx *ctx, RedisModuleString **argv, int argc,
                              long long *width, long long *depth) {
    long long size = 0;
    double overEst = 0, prob = 0;
    if (RMUtil_ArgIndex("ERROR", argv, argc) != 3) {
        INNER_ERROR("CMS: invalid ERROR");
    }
    if ((RedisModule_StringToDouble(argv[4], &overEst) != REDISMODULE_OK) || overEst <= 0 ||
        overEst >= 1) {
        INNER_ERROR("CMS: invalid over estimation value");
    }
    if ((RedisModule_StringToDouble(argv[5], &prob) != REDISMODULE_OK) ||
        (prob <= 0 || prob >= 1)) {
        INNER_ERROR("CMS: invalid prob value");
    }
    CMS_DimFromProb(size, overEst, prob, (size_t *)width, (size_t *)depth);

    return REDISMODULE_OK;
}

int CMSketch_CreateNew(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    if ((argc == 3 || argc == 6) == 0) {
        return RedisModule_WrongArity(ctx);
    }

    CMSketch *cms = NULL;
    long long width = 2.7, depth = 5, size = 0;
    RedisModuleString *keyName = argv[1];
    RedisModuleKey *key = RedisModule_OpenKey(ctx, keyName, REDISMODULE_READ | REDISMODULE_WRITE);

    if (RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_EMPTY) {
        RedisModule_CloseKey(key);
        return RedisModule_ReplyWithError(ctx, "CMS: key already exists");
    }

    if ((RedisModule_StringToLongLong(argv[2], &size) != REDISMODULE_OK) || (size < 1)) {
        INNER_ERROR("CMS: invalid n value");
    }

    CreateCMSKey(ctx, keyName, width * size, depth, &cms, &key);

    RedisModule_CloseKey(key);
    RedisModule_ReplyWithSimpleString(ctx, "OK");
    RedisModule_ReplicateVerbatim(ctx);
    return REDISMODULE_OK;
}
/*************************************************/

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
        CreateCMSKey(ctx, keyName, DEFAULT_WIDTH, DEFAULT_DEPTH, &cms, &key);
    } else if (RedisModule_ModuleTypeGetType(key) != CMSketchType) {
        return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    } else {
        cms = RedisModule_ModuleTypeGetValue(key);
    }

    int pairCount = (argc - 2) / 2;
    CMSPair *pairArray = CMS_CALLOC(pairCount, sizeof(CMSPair));
    parseIncrByArgs(ctx, argv, argc, &pairArray, pairCount);
    for (int i = 0; i < pairCount; ++i) {
        CMS_IncrBy(cms, pairArray[i].key, pairArray[i].keylen, pairArray[i].value);
    }

    CMS_FREE(pairArray);
    RedisModule_ReplyWithSimpleString(ctx, "OK");
    RedisModule_CloseKey(key);
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

static int parseMergeArgs(RedisModuleCtx *ctx, RedisModuleString **argv, int argc, CMSketch **dest,
                          CMSketch **cmsArray, long long *weights, long long numKeys) {
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

    if (GetCMSKey(ctx, argv[1], dest, REDISMODULE_READ | REDISMODULE_WRITE) != REDISMODULE_OK) {
        return REDISMODULE_ERR;
    }

    size_t width = (*dest)->width;
    size_t depth = (*dest)->depth;

    for (int i = 0; i < numKeys; ++i) {
        if (pos == -1) {
            weights[i] = 1;
        } else if (RedisModule_StringToLongLong(argv[3 + numKeys + 1 + i], &weights[i]) !=
                   REDISMODULE_OK) {
            INNER_ERROR("CMS: invalid weight value");
        }
        if (GetCMSKey(ctx, argv[3 + i], &cmsArray[i], REDISMODULE_READ) != REDISMODULE_OK) {
            return REDISMODULE_ERR;
        }
        if (cmsArray[i]->width != width || cmsArray[i]->depth != depth) {
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

    long long numKeys = 0;
    if (RedisModule_StringToLongLong(argv[2], &numKeys) != REDISMODULE_OK) {
        return RedisModule_ReplyWithError(ctx, "CMS: invalid numkeys");
    }

    CMSketch *dest = NULL;
    CMSketch **cmsArray = CMS_CALLOC(numKeys, sizeof(CMSketch *));
    long long *weights = CMS_CALLOC(numKeys, sizeof(long long));
    if (parseMergeArgs(ctx, argv, argc, &dest, cmsArray, weights, numKeys) != REDISMODULE_OK) {
        CMS_FREE(cmsArray);
        CMS_FREE(weights);
        return REDISMODULE_OK;
    }

    CMS_Merge(dest, numKeys, (const CMSketch **)cmsArray, (const long long *)weights);

    CMS_FREE(cmsArray);
    CMS_FREE(weights);
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

    RMUtil_RegisterWriteCmd(ctx, "cms.initbydim", CMSketch_Create);
    RMUtil_RegisterWriteCmd(ctx, "cms.initbyprob", CMSketch_Create);
    RMUtil_RegisterWriteCmd(ctx, "cms.reserve", CMSketch_CreateNew); // addition for new API
    RMUtil_RegisterWriteCmd(ctx, "cms.incrby", CMSketch_IncrBy);
    RMUtil_RegisterReadCmd(ctx, "cms.query", CMSketch_Query);
    RMUtil_RegisterWriteCmd(ctx, "cms.merge", CMSketch_Merge);
    RMUtil_RegisterReadCmd(ctx, "cms.info", CMSKetch_Info);
    //    RMUtil_RegisterReadCmd(ctx, "cms.gettopk", CMSketch_getTopK);
    //    RMUtil_RegisterReadCmd(ctx, "cms.findtopk", CMSketch_findTopK);

    return REDISMODULE_OK;
}