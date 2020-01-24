#include <math.h>    // ceil, log10f
#include <stdlib.h>  // malloc
#include <strings.h> // strncasecmp

#include "rmutil/util.h"
#include "version.h"

#include "contrib/agingBloom.h"
#include "rm_apbf.h"

#define INNER_ERROR(x)                      \
    RedisModule_ReplyWithError(ctx, x);     \
    return REDISMODULE_ERR;


#define APBF_CALLOC(count, size) RedisModule_Calloc(count, size)
#define APBF_FREE(ptr) RedisModule_Free(ptr)

RedisModuleType *APBFCountType;
RedisModuleType *APBFTimeType;

size_t APBFMemUsage(const void *value);

static int GetAPBFKey(RedisModuleCtx *ctx, RedisModuleString *keyName, ageBloom_t **apbf, int mode) {
    // All using this function should call RedisModule_AutoMemory to prevent memory leak
    RedisModuleKey *key = RedisModule_OpenKey(ctx, keyName, mode);
    if (RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY) {
        INNER_ERROR("APBF: key does not exist");
    } else if (RedisModule_ModuleTypeGetType(key) != APBFCountType) {
        INNER_ERROR(REDISMODULE_ERRORMSG_WRONGTYPE);
    }
    *apbf = RedisModule_ModuleTypeGetValue(key);
    return REDISMODULE_OK;
}

static int parseCreateArgs(RedisModuleCtx *ctx, RedisModuleString **argv, int argc,
                           long long *error, long long *capacity) {

    if ((RedisModule_StringToLongLong(argv[2], error) != REDISMODULE_OK) || *error < 1 || *error > 5) {
        INNER_ERROR("APBF: invalid error");
    }
    if ((RedisModule_StringToLongLong(argv[3], capacity) != REDISMODULE_OK) || *capacity < 32) {
        INNER_ERROR("APBF: invalid capacity or less than 32");
    }

    return REDISMODULE_OK;
}

int rmAPBF_Create(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    if (argc != 4) {
        return RedisModule_WrongArity(ctx);
    }

    ageBloom_t *apbf = NULL;
    long long error, capacity;
    RedisModuleString *keyName = argv[1];
    RedisModuleKey *key = RedisModule_OpenKey(ctx, keyName, REDISMODULE_READ | REDISMODULE_WRITE);

    if (RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_EMPTY) {
        RedisModule_CloseKey(key);
        return RedisModule_ReplyWithError(ctx, "APBF: key already exists");
    }

    if (parseCreateArgs(ctx, argv, argc, &error, &capacity) != REDISMODULE_OK)
        return REDISMODULE_OK;

    uint8_t level = 4;
    apbf = APBF_createHighLevelAPI(error, capacity, level);
    RedisModule_ModuleTypeSetValue(key, APBFCountType, apbf);

    RedisModule_CloseKey(key);
    RedisModule_ReplicateVerbatim(ctx);
    RedisModule_ReplyWithSimpleString(ctx, "OK");
    return REDISMODULE_OK;
}

int rmAPBF_Insert(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc < 3) {
        return RedisModule_WrongArity(ctx);
    }

    ageBloom_t *apbf = NULL;
    if (GetAPBFKey(ctx, argv[1], &apbf, REDISMODULE_WRITE) != REDISMODULE_OK) {
        return REDISMODULE_OK;
    }

    for (int i = 2; i < argc; ++i) {
        size_t strlen;
        const char *str = RedisModule_StringPtrLen(argv[i], &strlen);
        APBF_insertCount(apbf, str, strlen);
    }

    RedisModule_ReplicateVerbatim(ctx);
    RedisModule_ReplyWithSimpleString(ctx, "OK");
    return REDISMODULE_OK;
}

int rmAPBF_Query(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    if (argc < 3) {
        return RedisModule_WrongArity(ctx);
    }

    ageBloom_t *apbf = NULL;
    if (GetAPBFKey(ctx, argv[1], &apbf, REDISMODULE_READ) != REDISMODULE_OK) {
        return REDISMODULE_OK;
    }

    size_t strlen = 0;
    RedisModule_ReplyWithArray(ctx, argc - 2);
    for (int i = 2; i < argc; ++i) {
        const char *str = RedisModule_StringPtrLen(argv[i], &strlen);
        RedisModule_ReplyWithLongLong(ctx, APBF_query(apbf, str, strlen));
    }

    return REDISMODULE_OK;
}

int rmAPBF_Info(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    if (argc != 2)
        return RedisModule_WrongArity(ctx);

    ageBloom_t *apbf = NULL;
    if (GetAPBFKey(ctx, argv[1], &apbf, REDISMODULE_READ) != REDISMODULE_OK) {
        return REDISMODULE_OK;
    }

    RedisModule_ReplyWithArray(ctx, 7 * 2);
    RedisModule_ReplyWithSimpleString(ctx, "Size");
    RedisModule_ReplyWithLongLong(ctx, APBFMemUsage(apbf));
    RedisModule_ReplyWithSimpleString(ctx, "Capacity");
    RedisModule_ReplyWithLongLong(ctx, apbf->capacity);
    RedisModule_ReplyWithSimpleString(ctx, "Error rate");
    RedisModule_ReplyWithLongLong(ctx, apbf->errorRate);
    RedisModule_ReplyWithSimpleString(ctx, "Inserts count");
    RedisModule_ReplyWithLongLong(ctx, apbf->inserts);
    RedisModule_ReplyWithSimpleString(ctx, "Hash functions count");
    RedisModule_ReplyWithLongLong(ctx, apbf->numHash);
    RedisModule_ReplyWithSimpleString(ctx, "Periods count");
    RedisModule_ReplyWithLongLong(ctx, apbf->batches);
    RedisModule_ReplyWithSimpleString(ctx, "Slices count");
    RedisModule_ReplyWithLongLong(ctx, apbf->numSlices);

    return REDISMODULE_OK;
}

void APBFRdbSave(RedisModuleIO *io, void *obj) {
    ageBloom_t *apbf = obj;
    RedisModule_SaveUnsigned(io, apbf->numHash);
    RedisModule_SaveUnsigned(io, apbf->batches);
    RedisModule_SaveUnsigned(io, apbf->optimalSlices);
    RedisModule_SaveDouble(io, apbf->errorRate);
    RedisModule_SaveUnsigned(io, apbf->capacity);
    RedisModule_SaveUnsigned(io, apbf->inserts);
    RedisModule_SaveUnsigned(io, apbf->numSlices);
    RedisModule_SaveUnsigned(io, apbf->assessFreq);
    RedisModule_SaveStringBuffer(io, (const char *)apbf->slices,
                                 apbf->numSlices * sizeof(blmSlice));

    for (int i = 0; i < apbf->numSlices; ++i) {
        RedisModule_SaveStringBuffer(io, apbf->slices[i].data, apbf->slices[i].size);
    }
}

void *APBFRdbLoad(RedisModuleIO *io, int encver) {
    if (encver > APBF_ENC_VER) {
        return NULL;
    }

    ageBloom_t *apbf    = APBF_CALLOC(1, sizeof(ageBloom_t));
    apbf->numHash       = RedisModule_LoadUnsigned(io);
    apbf->batches       = RedisModule_LoadUnsigned(io);
    apbf->optimalSlices = RedisModule_LoadUnsigned(io);
    apbf->errorRate     = RedisModule_LoadDouble(io);
    apbf->capacity      = RedisModule_LoadUnsigned(io);
    apbf->inserts       = RedisModule_LoadUnsigned(io);
    apbf->numSlices     = RedisModule_LoadUnsigned(io);
    apbf->assessFreq    = RedisModule_LoadUnsigned(io);

    size_t length       = apbf->numSlices * sizeof(blmSlice);
    apbf->slices        = (blmSlice *)RedisModule_LoadStringBuffer(io, &length);

    for (int i = 0; i < apbf->numSlices; ++i) {
        apbf->slices[i].data = RedisModule_LoadStringBuffer(io, NULL);
    }

    return apbf;
}

void APBFFree(void *value) { APBF_destroy(value); }

size_t APBFMemUsage(const void *value) {
    ageBloom_t *apbf = (ageBloom_t *)value;
    size_t size = sizeof(*apbf) + apbf->numSlices * sizeof(blmSlice);

    for (int i = 0; i < apbf->numSlices; ++i) {
        size += apbf->slices[i].size / 8; // size is in bits, memUsage is in bytes
    }

    return size;
}

int APBFModule_onLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    // TODO: add option to set defaults from command line and in program
    RedisModuleTypeMethods tm = {.version = REDISMODULE_TYPE_METHOD_VERSION,
                                 .rdb_load = APBFRdbLoad,
                                 .rdb_save = APBFRdbSave,
                                 .aof_rewrite = RMUtil_DefaultAofRewrite,
                                 .mem_usage = APBFMemUsage,
                                 .free = APBFFree};

    APBFCountType = RedisModule_CreateDataType(ctx, "APBFcTYPE", APBF_ENC_VER, &tm);
    if (APBFCountType == NULL)
        return REDISMODULE_ERR;

    RMUtil_RegisterWriteDenyOOMCmd(ctx, "apbf.reserve", rmAPBF_Create);
    RMUtil_RegisterWriteDenyOOMCmd(ctx, "apbf.insert", rmAPBF_Insert);
    RMUtil_RegisterReadCmd(ctx, "apbf.query", rmAPBF_Query);
    RMUtil_RegisterReadCmd(ctx, "apbf.info", rmAPBF_Info);

    return REDISMODULE_OK;
}