#include <math.h>    // ceil, log10f
#include <stdlib.h>  // malloc
#include <strings.h> // strncasecmp

#include "rmutil/util.h"
#include "version.h"

#include "contrib/agingBloom.h"
#include "rm_apbf.h"

#define INNER_ERROR(x)                                                                             \
    RedisModule_ReplyWithError(ctx, x);                                                            \
    return REDISMODULE_ERR;

#define APBF_CALLOC(count, size) RedisModule_Calloc(count, size)
#define APBF_FREE(ptr) RedisModule_Free(ptr)

#define APBF_COUNT 1
#define APBF_TIME  2

#define APBF_DEFAULT_TIME_SIZE 1000

typedef struct {
    int error;
    int level;
    uint64_t capacity;
    timestamp_t timeSpan;
} createCtx;

RedisModuleType *APBFCountType;
RedisModuleType *APBFTimeType;

size_t APBFMemUsage(const void *value);

static int GetAPBFKey(RedisModuleCtx *ctx, RedisModuleString *keyName, ageBloom_t **apbf,
                      int *type, int mode) {
    // All using this function should call RedisModule_AutoMemory to prevent memory leak
    RedisModuleKey *key = RedisModule_OpenKey(ctx, keyName, mode);
    if (key == NULL || RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_MODULE) {
        INNER_ERROR("APBF: key does not exist");
    } 
    if (RedisModule_ModuleTypeGetType(key) == APBFCountType) {
        *type = APBF_COUNT;
    } else if (RedisModule_ModuleTypeGetType(key) == APBFTimeType) {
        *type = APBF_TIME;
    } else {
        INNER_ERROR(REDISMODULE_ERRORMSG_WRONGTYPE);
    }
    *apbf = RedisModule_ModuleTypeGetValue(key);
    return REDISMODULE_OK;
}

static int parseCreateArgs(RedisModuleCtx *ctx, RedisModuleString **argv, int argc,
                           long long *error, long long *capacity, long long *timeSpan) {

    if ((RedisModule_StringToLongLong(argv[2], error) != REDISMODULE_OK) || *error < 1 ||
        *error > 5) {
        INNER_ERROR("APBF: invalid error");
    }

    // A minimum capacity of 64 ensures that assert (capacity > l) will hold
    // for all (k,l) configurations, since max l is 63.
    // It is possible to allow smaller capacity by finner testing against actual l
    if ((RedisModule_StringToLongLong(argv[3], capacity) != REDISMODULE_OK) || *capacity < 64) {
        INNER_ERROR("APBF: invalid capacity or less than 64");
    }

    if (argc == 5) {
        if ((RedisModule_StringToLongLong(argv[4], timeSpan) != REDISMODULE_OK) || *capacity < 1) {
            INNER_ERROR("APBF: invalid time span");
        }
    }  
    /*
    // A minimum capacity of 64 ensures that assert (capacity > l) will hold
    // for all (k,l) configurations, since max l is 63.
    // It is possible to allow smaller capacity by finner testing against actual l
    if ((RedisModule_StringToLongLong(argv[2], level) != REDISMODULE_OK) || *level < 1 || *level > 5) {
        INNER_ERROR("APBF: invalid level. Range is 1 to 5");
    }*/

    return REDISMODULE_OK;
}

/*
 * APBF.CREATE idx Error Capacity Level
 */
int rmAPBF_Create(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    const char *cmd = RedisModule_StringPtrLen(argv[0], NULL);
    bool timeFilter = (cmd[4] == 't' || cmd[4] == 'T');
    
    if ((!timeFilter && argc != 4) || (timeFilter && argc != 5)) {
        return RedisModule_WrongArity(ctx);
    }

    ageBloom_t *apbf = NULL;
    long long error, capacity;
    long long level = 3;
    long long timeSpan;
    RedisModuleString *keyName = argv[1];
    RedisModuleKey *key = RedisModule_OpenKey(ctx, keyName, REDISMODULE_READ | REDISMODULE_WRITE);

    if (RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_EMPTY) {
        RedisModule_CloseKey(key);
        return RedisModule_ReplyWithError(ctx, "APBF: key already exists");
    }

    if (parseCreateArgs(ctx, argv, argc, &error, &capacity, &timeSpan) != REDISMODULE_OK)
        return REDISMODULE_OK;

    
    if (timeFilter == false) {  
        apbf = APBF_createHighLevelAPI(error, capacity, level);
        RedisModule_ModuleTypeSetValue(key, APBFCountType, apbf);
    } else {
        apbf = APBF_createTimeAPI(error, capacity, level, timeSpan);
        RedisModule_ModuleTypeSetValue(key, APBFTimeType, apbf);
    }

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

    int type = 0;
    ageBloom_t *apbf = NULL;
    if (GetAPBFKey(ctx, argv[1], &apbf, &type, REDISMODULE_WRITE) != REDISMODULE_OK) {
        return REDISMODULE_OK;
    }

    for (int i = 2; i < argc; ++i) {
        size_t strlen;
        const char *str = RedisModule_StringPtrLen(argv[i], &strlen);
        type == APBF_COUNT ? APBF_insertCount(apbf, str, strlen) :
                             APBF_insertTime(apbf, str, strlen);
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

    int type = 0;
    ageBloom_t *apbf = NULL;
    if (GetAPBFKey(ctx, argv[1], &apbf, &type, REDISMODULE_READ) != REDISMODULE_OK) {
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

    int type = 0;
    ageBloom_t *apbf = NULL;
    if (GetAPBFKey(ctx, argv[1], &apbf, &type, REDISMODULE_READ) != REDISMODULE_OK) {
        return REDISMODULE_OK;
    }

    RedisModule_ReplyWithArray(ctx, 9 * 2);
    RedisModule_ReplyWithSimpleString(ctx, "Type");
    RedisModule_ReplyWithSimpleString(ctx, (type == APBF_COUNT) ? "Age Partitioned Bloom Filter - Count":
                                                                  "Age Partitioned Bloom Filter - Time");
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
    RedisModule_ReplyWithSimpleString(ctx, "Time Span");
    RedisModule_ReplyWithLongLong(ctx, apbf->timeSpan);

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
    RedisModule_SaveUnsigned(io, apbf->timeSpan);
    RedisModule_SaveUnsigned(io, apbf->genSize);
    RedisModule_SaveUnsigned(io, apbf->counter);
    RedisModule_SaveUnsigned(io, apbf->currTime);
    RedisModule_SaveStringBuffer(io, (char *)&apbf->lastTimestamp, sizeof(timeval));
    
    RedisModule_SaveStringBuffer(io, (const char *)apbf->slices,
                                 apbf->numSlices * sizeof(blmSlice));

    for (int i = 0; i < apbf->numSlices; ++i) {
        blmSlice *slice = &apbf->slices[i];
        RedisModule_SaveUnsigned(io, slice->size);
        RedisModule_SaveUnsigned(io, slice->count);
        RedisModule_SaveUnsigned(io, slice->hashIndex);
        RedisModule_SaveUnsigned(io, slice->timestamp);
        RedisModule_SaveStringBuffer(io, apbf->slices[i].data, apbf->slices[i].size / 8 + 1);
    }
}

void *APBFRdbLoad(RedisModuleIO *io, int encver) {
    if (encver > APBF_ENC_VER) {
        return NULL;
    }

    ageBloom_t *apbf = APBF_CALLOC(1, sizeof(ageBloom_t));
    apbf->numHash = RedisModule_LoadUnsigned(io);
    apbf->batches = RedisModule_LoadUnsigned(io);
    apbf->optimalSlices = RedisModule_LoadUnsigned(io);
    apbf->errorRate = RedisModule_LoadDouble(io);
    apbf->capacity = RedisModule_LoadUnsigned(io);
    apbf->inserts = RedisModule_LoadUnsigned(io);
    apbf->numSlices = RedisModule_LoadUnsigned(io);
    apbf->timeSpan = RedisModule_LoadUnsigned(io);
    apbf->genSize = RedisModule_LoadUnsigned(io);
    apbf->counter = RedisModule_LoadUnsigned(io);
    apbf->currTime = RedisModule_LoadUnsigned(io);

    timeval time = *(timeval *)RedisModule_LoadStringBuffer(io, NULL);
    apbf->lastTimestamp = time;

    size_t length = apbf->numSlices * sizeof(blmSlice);
    apbf->slices = (blmSlice *)RedisModule_LoadStringBuffer(io, &length);

    for (int i = 0; i < apbf->numSlices; ++i) {
        blmSlice *slice = &apbf->slices[i];
        slice->size = RedisModule_LoadUnsigned(io);
        slice->count = RedisModule_LoadUnsigned(io);
        slice->hashIndex = RedisModule_LoadUnsigned(io);
        slice->timestamp = RedisModule_LoadUnsigned(io);
        slice->data = RedisModule_LoadStringBuffer(io, NULL);
    }

    return apbf;
}

void APBFFree(void *value) { APBF_destroy(value); }

size_t APBFMemUsage(const void *value) {
    ageBloom_t *apbf = (ageBloom_t *)value;
    size_t size = sizeof(*apbf);
    size += apbf->numSlices * sizeof(blmSlice);
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
    APBFTimeType = RedisModule_CreateDataType(ctx, "APBFtTYPE", APBF_ENC_VER, &tm);
    if (APBFTimeType == NULL)
        return REDISMODULE_ERR;

    // Counting filter
    RMUtil_RegisterWriteDenyOOMCmd(ctx, "apbfc.reserve", rmAPBF_Create);
    RMUtil_RegisterWriteDenyOOMCmd(ctx, "apbfc.add", rmAPBF_Insert);
    RMUtil_RegisterReadCmd(ctx, "apbfc.exists", rmAPBF_Query);
    RMUtil_RegisterReadCmd(ctx, "apbfc.info", rmAPBF_Info);

    // Counting filter
    RMUtil_RegisterWriteDenyOOMCmd(ctx, "apbft.reserve", rmAPBF_Create);
    RMUtil_RegisterWriteDenyOOMCmd(ctx, "apbft.add", rmAPBF_Insert);
    RMUtil_RegisterReadCmd(ctx, "apbft.exists", rmAPBF_Query);
    RMUtil_RegisterReadCmd(ctx, "apbft.info", rmAPBF_Info);

    return REDISMODULE_OK;
}
