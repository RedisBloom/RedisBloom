
#include <assert.h>

#include "version.h"
#include "rmutil/util.h"

#include "topk.h"
#include "rm_topk.h"

// clang-format off
#define INNER_ERROR(x) \
    do { \
        RedisModule_ReplyWithError(ctx, x); \
        return REDISMODULE_ERR; \
    } while(0)
// clang-format on

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

static int createTopK(RedisModuleCtx *ctx, RedisModuleString **argv, int argc, TopK **topk) {
    long long k, width, depth;
    double decay;
    if ((RedisModule_StringToLongLong(argv[2], &k) != REDISMODULE_OK) || k < 1) {
        INNER_ERROR("TopK: invalid k");
    }
    if (argc == 6) {
        if ((RedisModule_StringToLongLong(argv[3], &width) != REDISMODULE_OK) || width < 1 ||
            width > UINT32_MAX) {
            INNER_ERROR("TopK: invalid width");
        }
        if ((RedisModule_StringToLongLong(argv[4], &depth) != REDISMODULE_OK) || depth < 1 ||
            depth > UINT32_MAX) {
            INNER_ERROR("TopK: invalid depth");
        }
        if ((RedisModule_StringToDouble(argv[5], &decay) != REDISMODULE_OK) ||
            (decay <= 0 || decay > 1)) {
            INNER_ERROR("TopK: invalid decay value. must be '<= 1' & '> 0'");
        }
    } else {
        width = 8;
        depth = 7;
        decay = 0.9;
    }
    *topk = TopK_Create(k, width, depth, decay);
    if (!(*topk)) {
        INNER_ERROR("ERR Insufficient memory to create topk data structure");
    }
    return REDISMODULE_OK;
}

static int TopK_Create_Cmd(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc != 3 && argc != 6) {
        return RedisModule_WrongArity(ctx);
    }

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
    if (RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_EMPTY) {
        RedisModule_ReplyWithError(ctx, "TopK: key already exists");
        goto final;
    }

    TopK *topk = NULL;
    if (createTopK(ctx, argv, argc, &topk) != REDISMODULE_OK)
        goto final;

    if (RedisModule_ModuleTypeSetValue(key, TopKType, topk) == REDISMODULE_ERR) {
        goto final;
    }

    RedisModule_ReplicateVerbatim(ctx);
    RedisModule_ReplyWithSimpleString(ctx, "OK");
final:
    RedisModule_CloseKey(key);
    return REDISMODULE_OK;
}

static int TopK_Add_Cmd(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {

    if (argc < 3)
        return RedisModule_WrongArity(ctx);

    TopK *topk;
    if (GetTopKKey(ctx, argv[1], &topk, REDISMODULE_READ | REDISMODULE_WRITE) != REDISMODULE_OK) {
        return REDISMODULE_OK;
    }

    int itemCount = argc - 2;
    RedisModule_ReplyWithArray(ctx, itemCount);

    for (int i = 0; i < itemCount; ++i) {
        size_t itemlen;
        const char *item = RedisModule_StringPtrLen(argv[i + 2], &itemlen);
        char *expelledItem = TopK_Add(topk, item, itemlen, 1);

        if (expelledItem == NULL) {
            RedisModule_ReplyWithNull(ctx);
        } else {
            RedisModule_ReplyWithSimpleString(ctx, expelledItem);
            TOPK_FREE(expelledItem);
        }
    }
    RedisModule_ReplicateVerbatim(ctx);
    return REDISMODULE_OK;
}

static int TopK_Incrby_Cmd(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {

    if (argc < 4 || (argc % 2) == 1)
        return RedisModule_WrongArity(ctx);

    TopK *topk;
    if (GetTopKKey(ctx, argv[1], &topk, REDISMODULE_READ | REDISMODULE_WRITE) != REDISMODULE_OK) {
        return REDISMODULE_OK;
    }

    int itemCount = (argc - 2) / 2;
    RedisModule_ReplyWithArray(ctx, itemCount);

    for (int i = 0; i < itemCount; ++i) {
        size_t itemlen;
        const char *item = RedisModule_StringPtrLen(argv[2 + i * 2], &itemlen);
        long long increment;
        if (RedisModule_StringToLongLong(argv[2 + i * 2 + 1], &increment) || increment < 0 ||
            increment > 100000) {
            RedisModule_ReplyWithError(
                ctx, "TopK: increment must be an integer greater or equal to 0    \
                        and smaller or equal to 100,000");
            goto final;
        }
        char *expelledItem = TopK_Add(topk, item, itemlen, (uint32_t)increment);

        if (expelledItem == NULL) {
            RedisModule_ReplyWithNull(ctx);
        } else {
            RedisModule_ReplyWithSimpleString(ctx, expelledItem);
            TOPK_FREE(expelledItem);
        }
    }
final:
    RedisModule_ReplicateVerbatim(ctx);
    return REDISMODULE_OK;
}

static int TopK_Query_Cmd(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc < 3)
        return RedisModule_WrongArity(ctx);

    TopK *topk;
    if (GetTopKKey(ctx, argv[1], &topk, REDISMODULE_READ) != REDISMODULE_OK)
        return REDISMODULE_ERR;

    size_t itemlen;
    long long res;
    RedisModule_ReplyWithArray(ctx, argc - 2);
    for (int i = 2; i < argc; ++i) {
        const char *item = RedisModule_StringPtrLen(argv[i], &itemlen);
        res = TopK_Query(topk, item, itemlen);
        RedisModule_ReplyWithLongLong(ctx, res);
    }

    return REDISMODULE_OK;
}

static int TopK_Count_Cmd(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc < 3)
        return RedisModule_WrongArity(ctx);

    TopK *topk;
    if (GetTopKKey(ctx, argv[1], &topk, REDISMODULE_READ) != REDISMODULE_OK)
        return REDISMODULE_ERR;

    size_t itemlen;
    long long res;
    RedisModule_ReplyWithArray(ctx, argc - 2);
    for (int i = 2; i < argc; ++i) {
        const char *item = RedisModule_StringPtrLen(argv[i], &itemlen);
        res = TopK_Count(topk, item, itemlen);
        RedisModule_ReplyWithLongLong(ctx, res);
    }
    return REDISMODULE_OK;
}

static int TopK_List_Cmd(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc != 2 && argc != 3)
        return RedisModule_WrongArity(ctx);

    bool withCount = 0;
    if (argc == 3) {
        size_t len;
        const char *countStr = RedisModule_StringPtrLen(argv[2], &len);
        if (strncasecmp(countStr, "WITHCOUNT", len) == 0) {
            withCount = 1;
        } else {
            return RedisModule_ReplyWithError(ctx, "WITHCOUNT keyword expected");
        }
    }

    TopK *topk = NULL;
    if (GetTopKKey(ctx, argv[1], &topk, REDISMODULE_READ) != REDISMODULE_OK) {
        return REDISMODULE_OK;
    }
    HeapBucket *heapList = TopK_List(topk);
    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);
    long arrlen = 0;
    for (int i = 0; i < topk->k; ++i) {
        if (heapList[i].count != 0) {
            RedisModule_ReplyWithSimpleString(ctx, heapList[i].item);
            if (withCount) {
                RedisModule_ReplyWithLongLong(ctx, heapList[i].count);
            }
            arrlen += 1 + withCount;
        } else {
            break;
        }
    }
    RedisModule_ReplySetArrayLength(ctx, arrlen);

    TOPK_FREE(heapList);

    return REDISMODULE_OK;
}

static int TopK_Info_Cmd(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {

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

static void TopKRdbSave(RedisModuleIO *io, void *obj) {
    TopK *topk = obj;
    RedisModule_SaveUnsigned(io, topk->k);
    RedisModule_SaveUnsigned(io, topk->width);
    RedisModule_SaveUnsigned(io, topk->depth);
    RedisModule_SaveDouble(io, topk->decay);
    RedisModule_SaveStringBuffer(io, (const char *)topk->data,
                                 ((size_t)topk->width) * topk->depth * sizeof(Bucket));
    RedisModule_SaveStringBuffer(io, (const char *)topk->heap, topk->k * sizeof(HeapBucket));
    for (uint32_t i = 0; i < topk->k; ++i) {
        if (topk->heap[i].item != NULL) {
            RedisModule_SaveStringBuffer(io, topk->heap[i].item, strlen(topk->heap[i].item) + 1);
        } else {
            RedisModule_SaveStringBuffer(io, "", 1);
        }
    }
}

static void *TopKRdbLoad(RedisModuleIO *io, int encver) {
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
    assert(dataSize == ((size_t)topk->width) * topk->depth * sizeof(Bucket));
    topk->heap = (HeapBucket *)RedisModule_LoadStringBuffer(io, &heapSize);
    assert(heapSize == topk->k * sizeof(HeapBucket));
    for (uint32_t i = 0; i < topk->k; ++i) {
        topk->heap[i].item = RedisModule_LoadStringBuffer(io, &itemSize);
        if (itemSize == 1) {
            RedisModule_Free(topk->heap[i].item);
            topk->heap[i].item = NULL;
        }
    }

    return topk;
}

static void TopKFree(void *value) { TopK_Destroy(value); }

static size_t TopKMemUsage(const void *value) {
    TopK *topk = (TopK *)value;
    return sizeof(TopK) + ((size_t)topk->width) * topk->depth * sizeof(Bucket) +
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

    RMUtil_RegisterWriteDenyOOMCmd(ctx, "topk.reserve", TopK_Create_Cmd);
    RMUtil_RegisterWriteDenyOOMCmd(ctx, "topk.add", TopK_Add_Cmd);
    RMUtil_RegisterWriteDenyOOMCmd(ctx, "topk.incrby", TopK_Incrby_Cmd);
    RMUtil_RegisterReadCmd(ctx, "topk.query", TopK_Query_Cmd);
    RMUtil_RegisterWriteCmd(ctx, "topk.count", TopK_Count_Cmd);
    RMUtil_RegisterReadCmd(ctx, "topk.list", TopK_List_Cmd);
    RMUtil_RegisterReadCmd(ctx, "topk.info", TopK_Info_Cmd);

    return REDISMODULE_OK;
}
