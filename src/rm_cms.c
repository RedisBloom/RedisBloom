#include <strings.h>        // strncasecmp
#include <stdlib.h>         // malloc
#include <math.h>           // ceil, log10f

#include "version.h"
#include "rmutil/util.h"

#include "rm_cms.h"
#include "cms.h"

RedisModuleType *CMSketchType;

typedef struct {
    const char *key;
    long long value;
} CMSPair;

static int CompareCStringToRedisString(char *str, RedisModuleString *redisStr) {
    size_t len;
    char *cRedisStr = (char *)RedisModule_StringPtrLen(redisStr, &len);
    return strncasecmp(cRedisStr, str, len) == 0;
}

static int GetCMS(RedisModuleCtx *ctx, RedisModuleString *keyName, 
                    CMSketch **cms, int mode) {
    RedisModuleKey *key = RedisModule_OpenKey(ctx, keyName, mode);
    if(RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY) {
        RedisModule_ReplyWithError(ctx, "CMS: key does not exist");
        return REDISMODULE_ERR;
    } else if(RedisModule_ModuleTypeGetType(key) != CMSketchType) {
        RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
        return REDISMODULE_ERR;
    } 
    *cms = RedisModule_ModuleTypeGetValue(key);
    return REDISMODULE_OK;
}

static int CreateCmsKey(RedisModuleCtx *ctx, RedisModuleString *keyName, 
        long long width, long long depth, CMSketch **cms, RedisModuleKey **key) {
    if(*key == NULL) {
        *key = RedisModule_OpenKey(ctx, keyName, REDISMODULE_READ|REDISMODULE_WRITE);
    }

    RedisModule_RetainString(ctx, keyName);
    *cms = NewCMSketch(width, depth);

    if(RedisModule_ModuleTypeSetValue(*key, CMSketchType, *cms) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}

static int parseCreateArgs(RedisModuleCtx *ctx, RedisModuleString **argv, int argc,
                    long long *width, long long *depth) {
    if(CompareCStringToRedisString("cms.initbydim", argv[0]) == 1) {
        if((RedisModule_StringToLongLong(argv[2], width) != REDISMODULE_OK)) 
            return RedisModule_ReplyWithError(ctx, "CMS: invalid value");
        if((RedisModule_StringToLongLong(argv[3], depth) != REDISMODULE_OK)) 
            return RedisModule_ReplyWithError(ctx, "CMS: invalid value");        
    } else {
        double err, prob;
        if((RedisModule_StringToDouble(argv[2], &err) != REDISMODULE_OK)) 
            return RedisModule_ReplyWithError(ctx, "CMS: invalid value");
        if((RedisModule_StringToDouble(argv[3], &prob) != REDISMODULE_OK)) 
            return RedisModule_ReplyWithError(ctx, "CMS: invalid value");  

        *width = ceil(2 / err);
        *depth = ceil(log10f(prob) / log10f(0.5));
    }
    return REDISMODULE_OK;
}

int CMSketch_create(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if(argc != 4)
        return RedisModule_WrongArity(ctx);

    CMSketch *cms;
    long long width, depth;
    RedisModuleString *keyName = argv[1];
    RedisModuleKey *key = RedisModule_OpenKey(ctx, keyName, REDISMODULE_READ|REDISMODULE_WRITE);

    if(parseCreateArgs(ctx, argv, argc, &width, &depth) != REDISMODULE_OK)
        return REDISMODULE_ERR;

    if(RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_EMPTY) {
        RedisModule_CloseKey(key);
        return RedisModule_ReplyWithError(ctx,"CMS: key already exists");
    }

    CreateCmsKey(ctx, keyName, width, depth, &cms, &key);
    RedisModule_CloseKey(key);

    RedisModule_Log(ctx, "info", "created new count-min sketch");
    RedisModule_ReplyWithSimpleString(ctx, "OK");
    RedisModule_ReplicateVerbatim(ctx);
    return REDISMODULE_OK;
}

static int parseIncrByArgs(RedisModuleCtx *ctx, RedisModuleString **argv, int argc,
                CMSPair **pairs, int qty) {
    *pairs = (CMSPair *)CMS_CALLOC(qty, sizeof(CMSPair));
    for(int i = 0; i < qty; ++i) {
        pairs[i]->key = RedisModule_StringPtrLen(argv[2 + i * 2], NULL);
        RedisModule_StringToLongLong(argv[2 + i * 2 + 1], &(pairs[i]->value));
    }
    return REDISMODULE_OK;
}

int CMSketch_incrBy(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

   if(argc < 4 || (argc % 2) == 1) {
        return RedisModule_WrongArity(ctx);
    }

    int pairCount = (argc - 2) / 2;
    RedisModuleString *keyName = argv[1];
    RedisModuleKey *key = RedisModule_OpenKey(ctx, keyName, REDISMODULE_READ);
    CMSketch *cms;
    CMSPair *pairArray;
    parseIncrByArgs(ctx, argv, argc, &pairArray, pairCount);

    if(RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY) {
        CreateCmsKey(ctx, keyName, DEFAULT_WIDTH, DEFAULT_DEPTH, &cms, &key);
    } else if(RedisModule_ModuleTypeGetType(key) != CMSketchType) {
        return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    } else {
        cms = RedisModule_ModuleTypeGetValue(key);
    }
 
    for(int i = 0; i < pairCount; ++i) {
        CMS_IncrBy(cms,pairArray[i].key, pairArray[i].value);
    }    

    CMS_FREE(pairArray);
    RedisModule_ReplyWithSimpleString(ctx, "OK");
    RedisModule_CloseKey(key);
    return REDISMODULE_OK;
}

int CMSketch_query(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if(argc < 3) {
        return RedisModule_WrongArity(ctx);
    }

    CMSketch *cms = NULL;
    if(GetCMS(ctx, argv[1], &cms, REDISMODULE_READ) != REDISMODULE_OK) {
        return REDISMODULE_ERR;
    }

    int itemCount = argc - 2;
    size_t length;
    RedisModule_ReplyWithArray(ctx, itemCount);
    for(int i = 0; i < itemCount; ++i) {
        const char *str = RedisModule_StringPtrLen(argv[2 + i], &length);
        RedisModule_ReplyWithLongLong(ctx, CMS_Query(cms, str));
    }

    return REDISMODULE_OK;
}

static int parseMergeArgs(RedisModuleCtx *ctx, RedisModuleString **argv,
                            int argc, CMSketch **cmsArray, long long *weights,
                            long long numKeys, int pos) {
    for(int i = 0; i < numKeys; ++i) {
        if(pos > 0) {
            if(RedisModule_StringToLongLong(argv[3 + numKeys + 1 + i], &weights[i]) !=
                REDISMODULE_OK) {
                return RedisModule_ReplyWithError(ctx, "CMS: invalid weight value");
            }
        } else {
            weights[i] = 1; 
        }
        if(GetCMS(ctx, argv[3 + i], &cmsArray[i], REDISMODULE_READ) != REDISMODULE_OK) {
           return REDISMODULE_ERR;
        }
    }

    return REDISMODULE_OK;
}


int CMSketch_merge(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if(argc < 4) {
        return RedisModule_WrongArity(ctx);
    }   

    long long numKeys;
    int pos = RMUtil_ArgIndex("WEIGHTS", argv, argc);
    if(RedisModule_StringToLongLong(argv[2], &numKeys) != REDISMODULE_OK) {
        return RedisModule_ReplyWithError(ctx, "CMS: invalid numkeys");
    }
    if(pos < 0) {
        if(numKeys != argc - 3)
            return RedisModule_ReplyWithError(ctx, "CMS: wrong number of keys");
    } else {
        if((pos != 3 + numKeys) && (argc != 4 + numKeys * 2)) 
            return RedisModule_ReplyWithError(ctx, 
                        "CMS: wrong number of keys/weights");
    }

    CMSketch *dest;
    if(GetCMS(ctx, argv[1], &dest, REDISMODULE_READ|REDISMODULE_WRITE) != REDISMODULE_OK) {
        return REDISMODULE_ERR;
    }    
    CMSketch **cmsArray = CMS_CALLOC(numKeys, sizeof(CMSketch *));
    long long *weights = CMS_CALLOC(numKeys, sizeof(long long));
    if(parseMergeArgs(ctx, argv, argc, cmsArray, weights, numKeys, pos) != REDISMODULE_OK) {
        CMS_FREE(cmsArray);
        CMS_FREE(weights);
        return REDISMODULE_ERR;
    }
    
    CMS_Merge(dest, numKeys, cmsArray, weights);

    CMS_FREE(cmsArray);
    CMS_FREE(weights);
    RedisModule_ReplyWithSimpleString(ctx, "OK");
    return REDISMODULE_OK;
}

int CMSKetch_info(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    
    if (argc != 2) return RedisModule_WrongArity(ctx);

    CMSketch *cms = NULL;    
    if(GetCMS(ctx, argv[1], &cms, REDISMODULE_READ) != REDISMODULE_OK) {
        return REDISMODULE_ERR;
    }

    RedisModule_ReplyWithArray(ctx, 3*2);
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
                                cms->width * cms->depth * sizeof(size_t));
}

void *CMSRdbLoad(RedisModuleIO *io, int encver) {
    if(encver > CMS_ENC_VER) {
        return NULL;
    }

    CMSketch *cms = CMS_CALLOC(1, sizeof(CMSketch));
    cms->width = RedisModule_LoadUnsigned(io);
    cms->depth = RedisModule_LoadUnsigned(io);
    cms->counter = RedisModule_LoadUnsigned(io);
    size_t length = cms->width * cms->depth * sizeof(size_t);
    cms->array = (size_t *)RedisModule_LoadStringBuffer(io, &length);

    return cms;
}

void CMSFree(void *value) { 
    CMS_Destroy(value);
}

size_t CMSMemUsage(const void *value) {
    CMSketch *cms = (CMSketch *)value;
    return sizeof(cms) + cms->width * cms->depth * sizeof(size_t);
}

void AofTemp(RedisModuleIO *aof, RedisModuleString *key, void *value) {

}

int CMSModule_onLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModuleTypeMethods tm = {
            .version = REDISMODULE_TYPE_METHOD_VERSION,
            .rdb_load = CMSRdbLoad,
            .rdb_save = CMSRdbSave,
            .aof_rewrite = AofTemp, 
            .aof_rewrite = RMUtil_DefaultAofRewrite, 
            .mem_usage = CMSMemUsage,
            .free = CMSFree
        };

    CMSketchType = RedisModule_CreateDataType(ctx, "CMSk-TYPE", CMS_ENC_VER, &tm);
    if(CMSketchType == NULL) return REDISMODULE_ERR;

    RMUtil_RegisterWriteCmd(ctx, "cms.initbydim", CMSketch_create);
    RMUtil_RegisterWriteCmd(ctx, "cms.initbyprob", CMSketch_create);
    RMUtil_RegisterWriteCmd(ctx, "cms.incrby", CMSketch_incrBy);
    RMUtil_RegisterReadCmd(ctx, "cms.query", CMSketch_query);
    RMUtil_RegisterWriteCmd(ctx, "cms.merge", CMSketch_merge);
    RMUtil_RegisterReadCmd(ctx, "cms.info", CMSKetch_info);
//    RMUtil_RegisterReadCmd(ctx, "cms.gettopk", CMSketch_getTopK);
//    RMUtil_RegisterReadCmd(ctx, "cms.findtopk", CMSketch_findTopK);

    return REDISMODULE_OK;
}