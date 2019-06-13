//#include <math.h>    // ceil, log10f
//#include <strings.h> // strncasecmp
#include <assert.h>

#include "version.h"
#include "rmutil/util.h"

#include "topk.h"
#include "rm_topk.h"

#define INNER_ERROR(x)                                                                             \
    RedisModule_ReplyWithError(ctx, x);                                                            \
    return REDISMODULE_ERR;

RedisModuleType *TopKType;

static int GetTopKKey(RedisModuleCtx *ctx, RedisModuleString *keyName, TopK **topk, int mode) {
    // All using this function should call RedisModule_AutoMemory to prevent memory leak
    RedisModuleKey *key = RedisModule_OpenKey(ctx, keyName, mode);
    if (RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY) {
        RedisModule_CloseKey(key);
        INNER_ERROR("TopK: key does not exist");
    } else if (RedisModule_ModuleTypeGetType(key) != TopKType) {
        RedisModule_CloseKey(key);
        INNER_ERROR(REDISMODULE_ERRORMSG_WRONGTYPE);
    }

    *topk = RedisModule_ModuleTypeGetValue(key);
    RedisModule_CloseKey(key);
    return REDISMODULE_OK;
}

static int createTopK(RedisModuleCtx *ctx, RedisModuleString **argv, TopK **topk) {
    long long k, width, depth;
    double decay;
    if ((RedisModule_StringToLongLong(argv[2], &k) != REDISMODULE_OK) || k < 1) {
        INNER_ERROR("TopK: invalid k");
    }
    if ((RedisModule_StringToLongLong(argv[3], &width) != REDISMODULE_OK) || width < 1) {
        INNER_ERROR("TopK: invalid width");
    }
    if ((RedisModule_StringToLongLong(argv[4], &depth) != REDISMODULE_OK) || depth < 1) {
        INNER_ERROR("TopK: invalid depth");
    }    
    if ((RedisModule_StringToDouble(argv[5], &decay) != REDISMODULE_OK) ||
                                    (decay <= 0 || decay >= 1)) {
        INNER_ERROR("TopK: invalid decay value. must be '< 1' & '> 0'");
    }

    *topk = TopK_Create(k, width, depth, decay);
    return REDISMODULE_OK;
}

int TopK_Create_Cmd(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc != 6) {
        return RedisModule_WrongArity(ctx);
    }

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
    if (RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_EMPTY) {
        RedisModule_ReplyWithError(ctx, "TopK: key already exists");
        goto final;
    }

    TopK *topk = NULL;
    if (createTopK(ctx, argv, &topk) != REDISMODULE_OK)
        goto final;

    if (RedisModule_ModuleTypeSetValue(key, TopKType, topk) == REDISMODULE_ERR) {
        goto final;
    }

    RedisModule_ReplyWithSimpleString(ctx, "OK");
final:    
    RedisModule_CloseKey(key);
    return REDISMODULE_OK;
}

int TopK_Add_Cmd(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
//    RedisModule_AutoMemory(ctx);
    if (argc < 3)
        return RedisModule_WrongArity(ctx);

    TopK *topk;
    if(GetTopKKey(ctx, argv[1], &topk,  REDISMODULE_READ | REDISMODULE_WRITE) != 
                            REDISMODULE_OK) {
        return REDISMODULE_OK;
    }

    size_t itemlen;
    for(int i = 2; i < argc; ++i) {
        const char *item = RedisModule_StringPtrLen(argv[i], &itemlen);
        TopK_Add(topk, item, itemlen);
    }
    
    RedisModule_ReplyWithSimpleString(ctx, "OK");
    return REDISMODULE_OK;
}

int TopK_Query_Cmd(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
//    RedisModule_AutoMemory(ctx);    
    if (argc < 3)
        return RedisModule_WrongArity(ctx);
    
    TopK *topk;
    if(GetTopKKey(ctx, argv[1], &topk, REDISMODULE_READ) != REDISMODULE_OK)
        return REDISMODULE_ERR;

    size_t itemlen;
    long long res;
    RedisModule_ReplyWithArray(ctx, argc - 2);
    for(int i = 2; i < argc; ++i) {
        const char *item = RedisModule_StringPtrLen(argv[i], &itemlen);
        res = TopK_Query(topk, item, itemlen);
        RedisModule_ReplyWithLongLong(ctx, res);
    }
    
    return REDISMODULE_OK;
}

int TopK_Count_Cmd(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
//   RedisModule_AutoMemory(ctx);    
    if (argc < 3)
        return RedisModule_WrongArity(ctx);
    
    TopK *topk;
    if(GetTopKKey(ctx, argv[1], &topk, REDISMODULE_READ) != REDISMODULE_OK)
        return REDISMODULE_ERR;

    size_t itemlen;
    long long res;
    RedisModule_ReplyWithArray(ctx, argc - 2);
    for(int i = 2; i < argc; ++i) {
        const char *item = RedisModule_StringPtrLen(argv[i], &itemlen);
        res = TopK_Count(topk, item, itemlen);
        RedisModule_ReplyWithLongLong(ctx, res);
    }
    
    return REDISMODULE_OK;
}

int TopK_List_Cmd(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
//    RedisModule_AutoMemory(ctx);
    if (argc != 2)
        return RedisModule_WrongArity(ctx);

    TopK *topk = NULL;
    if (GetTopKKey(ctx, argv[1], &topk, REDISMODULE_READ) != REDISMODULE_OK) {
        return REDISMODULE_OK;
    }
    uint32_t k = topk->k;
    char **heapList = TOPK_CALLOC(k, (sizeof(char *)));
    TopK_List(topk, heapList);
    RedisModule_ReplyWithArray(ctx, k);
    for(int i = 0; i < k; ++i) {
        if(heapList[i] != NULL) {
            RedisModule_ReplyWithSimpleString(ctx, heapList[i]);
        } else  {
            RedisModule_ReplyWithNull(ctx);
        }
    }
    TOPK_FREE(heapList);

    return REDISMODULE_OK;
}

int TopK_Info_Cmd(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
//    RedisModule_AutoMemory(ctx);
    if (argc != 2)
        return RedisModule_WrongArity(ctx);

    TopK *topk = NULL;
    if (GetTopKKey(ctx, argv[1], &topk, REDISMODULE_READ) != REDISMODULE_OK) {
        return REDISMODULE_OK;
    }

    RedisModule_ReplyWithArray(ctx, 4 * 2);
    RedisModule_ReplyWithSimpleString(ctx, "k");
    RedisModule_ReplyWithLongLong(ctx, topk->k);
    RedisModule_ReplyWithSimpleString(ctx, "width");
    RedisModule_ReplyWithLongLong(ctx, topk->width);
    RedisModule_ReplyWithSimpleString(ctx, "depth");
    RedisModule_ReplyWithLongLong(ctx, topk->depth);
    RedisModule_ReplyWithSimpleString(ctx, "decay");
    RedisModule_ReplyWithDouble(ctx, topk->decay);

    return REDISMODULE_OK;
}

/**************** Module functions *********************************/

void TopKRdbSave(RedisModuleIO *io, void *obj) {
    TopK *topk = obj;
    RedisModule_SaveUnsigned(io, topk->k);
    RedisModule_SaveUnsigned(io, topk->width);
    RedisModule_SaveUnsigned(io, topk->depth);
    RedisModule_SaveDouble(io, topk->decay);
    RedisModule_SaveStringBuffer(io, (const char *)topk->data,
                                 topk->width * topk->depth * sizeof(Bucket));
    RedisModule_SaveStringBuffer(io, (const char *)topk->heap,
                                 topk->k * sizeof(HeapBucket));
    for(uint32_t i = 0; i < topk->k; ++i) {
        if(topk->heap[i].item != NULL) {
            RedisModule_SaveStringBuffer(io, topk->heap[i].item, strlen(topk->heap[i].item) + 1);
        } else {
            RedisModule_SaveStringBuffer(io, "", 1);
        }
    }
}

void *TopKRdbLoad(RedisModuleIO *io, int encver) {
    if (encver > TOPK_ENC_VER) {
        return NULL;
    }
    TopK *topk = TOPK_CALLOC(1, sizeof(TopK));
    topk->k = RedisModule_LoadUnsigned(io);
    topk->width = RedisModule_LoadUnsigned(io);
    topk->depth = RedisModule_LoadUnsigned(io);
    topk->decay = RedisModule_LoadDouble(io);
  
    size_t dataSize, heapSize, itemSize;
    topk->data = (Bucket *)RedisModule_LoadStringBuffer(io, &dataSize);
    assert(dataSize == topk->width * topk->depth * sizeof(Bucket));
    topk->heap = (HeapBucket *)RedisModule_LoadStringBuffer(io, &heapSize);
    assert(heapSize == topk->k * sizeof(HeapBucket));
    for(uint32_t i = 0; i < topk->k; ++i) {
        topk->heap[i].item = RedisModule_LoadStringBuffer(io, &itemSize);
        if(itemSize == 1) {
            topk->heap[i].item = NULL;
        }
    }

    return topk;
}

void TopKFree(void *value) { TopK_Destroy(value); }

size_t TopKMemUsage(const void *value) {
    TopK *topk = (TopK *)value;
    return sizeof(TopK) + 
            topk->width * topk->depth * sizeof(Bucket) + 
            topk->k * sizeof(HeapBucket);
}

int TopKModule_onLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    // TODO: add option to set defaults from command line and in program
    RedisModuleTypeMethods tm = {.version = REDISMODULE_TYPE_METHOD_VERSION,
                                 .rdb_load = TopKRdbLoad,
                                 .rdb_save = TopKRdbSave,
                                 .aof_rewrite = RMUtil_DefaultAofRewrite,
                                 .mem_usage = TopKMemUsage,
                                 .free = TopKFree};

    TopKType = RedisModule_CreateDataType(ctx, "TopK-TYPE", TOPK_ENC_VER, &tm);
    if (TopKType == NULL)
        return REDISMODULE_ERR;

    RMUtil_RegisterWriteCmd(ctx, "topk.reserve", TopK_Create_Cmd);
    RMUtil_RegisterWriteCmd(ctx, "topk.add", TopK_Add_Cmd);
    RMUtil_RegisterReadCmd(ctx, "topk.query", TopK_Query_Cmd);
    RMUtil_RegisterWriteCmd(ctx, "topk.count", TopK_Count_Cmd);
    RMUtil_RegisterReadCmd(ctx, "topk.list", TopK_List_Cmd);
    RMUtil_RegisterReadCmd(ctx, "topk.info", TopK_Info_Cmd);

    return REDISMODULE_OK;
}