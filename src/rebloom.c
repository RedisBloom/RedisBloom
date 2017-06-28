#include "rebloom.h"
#include "redismodule.h"
#define BLOOM_CALLOC RedisModule_Calloc
#define BLOOM_FREE RedisModule_Free
#include "contrib/bloom.c"
#include <string.h>
#include <strings.h> // strncasecmp

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// Core                                                                     ///
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
#define ERROR_TIGHTENING_RATIO 0.5
#define CUR_FILTER(sb) ((sb)->filters + ((sb)->nfilters - 1))

static void SBChain_AddLink(SBChain *chain, size_t size, double error_rate) {
    if (!chain->filters) {
        chain->filters = RedisModule_Calloc(1, sizeof(*chain->filters));
    } else {
        chain->filters =
            RedisModule_Realloc(chain->filters, sizeof(*chain->filters) * (chain->nfilters + 1));
    }

    SBLink *newlink = chain->filters + chain->nfilters;
    newlink->size = 0;
    chain->nfilters++;
    bloom_init(&newlink->inner, size, error_rate);
}

void SBChain_Free(SBChain *sb) {
    for (size_t ii = 0; ii < sb->nfilters; ++ii) {
        bloom_free(&sb->filters[ii].inner);
    }
    RedisModule_Free(sb->filters);
    RedisModule_Free(sb);
}

static int SBChain_AddToLink(SBLink *lb, const void *data, size_t len, bloom_hashval hash) {
    if (!bloom_add_h(&lb->inner, data, len, hash)) {
        // Element not previously present?
        lb->size++;
        return 1;
    } else {
        return 0;
    }
}

int SBChain_Add(SBChain *sb, const void *data, size_t len) {
    // Does it already exist?

    bloom_hashval h = bloom_calc_hash(data, len);
    for (int ii = sb->nfilters - 1; ii >= 0; --ii) {
        if (bloom_check_h(&sb->filters[ii].inner, data, len, h)) {
            return 0;
        }
    }

    // Determine if we need to add more items?
    SBLink *cur = CUR_FILTER(sb);
    if (cur->size >= cur->inner.entries) {
        double error = cur->inner.error * pow(ERROR_TIGHTENING_RATIO, sb->nfilters + 1);
        SBChain_AddLink(sb, cur->inner.entries * 2, error);
        cur = CUR_FILTER(sb);
    }

    int rv = SBChain_AddToLink(cur, data, len, h);
    if (rv) {
        sb->size++;
    }
    return rv;
}

int SBChain_Check(const SBChain *sb, const void *data, size_t len) {
    bloom_hashval hv = bloom_calc_hash(data, len);
    for (int ii = sb->nfilters - 1; ii >= 0; --ii) {
        if (bloom_check_h(&sb->filters[ii].inner, data, len, hv)) {
            return 1;
        }
    }
    return 0;
}

SBChain *SB_NewChain(size_t initsize, double error_rate) {
    if (initsize == 0 || error_rate == 0) {
        return NULL;
    }
    SBChain *sb = RedisModule_Calloc(1, sizeof(*sb));
    SBChain_AddLink(sb, initsize, error_rate);
    return sb;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// Redis Commands                                                           ///
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
static RedisModuleType *BFType;
static double BFDefaultErrorRate = 0.01;
static size_t BFDefaultInitCapacity = 100;

typedef enum { SB_OK = 0, SB_MISSING, SB_EMPTY, SB_MISMATCH } lookupStatus;

static int bfGetChain(RedisModuleKey *key, SBChain **sbout) {
    *sbout = NULL;
    if (key == NULL) {
        return SB_MISSING;
    }
    int type = RedisModule_KeyType(key);
    if (type == REDISMODULE_KEYTYPE_EMPTY) {
        return SB_EMPTY;
    } else if (type == REDISMODULE_KEYTYPE_MODULE && RedisModule_ModuleTypeGetType(key) == BFType) {
        *sbout = RedisModule_ModuleTypeGetValue(key);
        return SB_OK;
    } else {
        return SB_MISMATCH;
    }
}

static const char *statusStrerror(int status) {
    switch (status) {
    case SB_MISSING:
        return "ERR not found";
    case SB_MISMATCH:
        return REDISMODULE_ERRORMSG_WRONGTYPE;
    case SB_OK:
        return "ERR item exists";
    default:
        return "Unknown error";
    }
}

/**
 * Common function for adding one or more items to a bloom filter.
 * capacity and error rate must not be 0.
 */
static SBChain *bfCreateChain(RedisModuleKey *key, double error_rate, size_t capacity) {
    SBChain *sb = SB_NewChain(capacity, error_rate);
    RedisModule_ModuleTypeSetValue(key, BFType, sb);
    return sb;
}

/**
 * Reserves a new empty filter with custom parameters:
 * BF.CREATE <KEY> <ERROR_RATE (double)> <INITIAL_CAPACITY (int)>
 */
static int BFReserve_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc != 4) {
        RedisModule_WrongArity(ctx);
        return REDISMODULE_ERR;
    }

    double error_rate;
    if (RedisModule_StringToDouble(argv[2], &error_rate) != REDISMODULE_OK) {
        return RedisModule_ReplyWithError(ctx, "ERR bad error rate");
    }

    long long capacity;
    if (RedisModule_StringToLongLong(argv[3], &capacity) != REDISMODULE_OK) {
        return RedisModule_ReplyWithError(ctx, "ERR bad capacity");
    }

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
    SBChain *sb;
    int status = bfGetChain(key, &sb);
    if (status != SB_EMPTY) {
        return RedisModule_ReplyWithError(ctx, statusStrerror(status));
    }

    bfCreateChain(key, error_rate, capacity);
    RedisModule_ReplyWithSimpleString(ctx, "OK");
    return REDISMODULE_OK;
}

/**
 * Check for the existence of an item
 * BF.TEST <KEY>
 * Returns true or false
 */
static int BFCheck_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc != 3) {
        RedisModule_WrongArity(ctx);
        return REDISMODULE_ERR;
    }

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
    SBChain *sb;
    int status = bfGetChain(key, &sb);
    if (status != SB_OK) {
        return RedisModule_ReplyWithError(ctx, statusStrerror(status));
    }

    // Check if it exists?
    size_t n;
    const char *s = RedisModule_StringPtrLen(argv[2], &n);
    int exists = SBChain_Check(sb, s, n);
    RedisModule_ReplyWithLongLong(ctx, exists);
    return REDISMODULE_OK;
}

/**
 * Adds items to an existing filter. Creates a new one on demand if it doesn't exist.
 * BF.SET <KEY> ITEMS...
 * Returns an array of integers. The nth element is either 1 or 0 depending on whether it was newly
 * added, or had previously existed, respectively.
 */
static int BFAdd_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc < 3) {
        RedisModule_WrongArity(ctx);
        return REDISMODULE_ERR;
    }

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
    SBChain *sb;
    int status = bfGetChain(key, &sb);
    if (status == SB_EMPTY) {
        sb = bfCreateChain(key, BFDefaultErrorRate, BFDefaultInitCapacity);
    } else if (status != SB_OK) {
        return RedisModule_ReplyWithError(ctx, statusStrerror(status));
    }

    RedisModule_ReplyWithArray(ctx, argc - 2);

    for (size_t ii = 2; ii < argc; ++ii) {
        size_t n;
        const char *s = RedisModule_StringPtrLen(argv[ii], &n);
        int rv = SBChain_Add(sb, s, n);
        RedisModule_ReplyWithLongLong(ctx, !!rv);
    }
    return REDISMODULE_OK;
}

/**
 * BF.DEBUG KEY
 * returns some information about the bloom filter.
 */
static int BFInfo_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc != 2) {
        RedisModule_WrongArity(ctx);
        return REDISMODULE_ERR;
    }

    const SBChain *sb = NULL;
    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
    int status = bfGetChain(key, (SBChain **)&sb);
    if (status != SB_OK) {
        return RedisModule_ReplyWithError(ctx, statusStrerror(status));
    }

    // Start writing info
    RedisModule_ReplyWithArray(ctx, 1 + sb->nfilters);

    RedisModuleString *info_s = RedisModule_CreateStringPrintf(ctx, "size:%llu", sb->size);
    RedisModule_ReplyWithString(ctx, info_s);
    RedisModule_FreeString(ctx, info_s);

    for (size_t ii = 0; ii < sb->nfilters; ++ii) {
        const SBLink *lb = sb->filters + ii;
        info_s = RedisModule_CreateStringPrintf(
            ctx, "bytes:%d bits:%d hashes:%d capacity:%d size:%lu ratio:%g", lb->inner.bytes,
            lb->inner.bits, lb->inner.hashes, lb->inner.entries, lb->size, lb->inner.error);
        RedisModule_ReplyWithString(ctx, info_s);
        RedisModule_FreeString(ctx, info_s);
    }

    return REDISMODULE_OK;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// Datatype Functions                                                       ///
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
static void BFRdbSave(RedisModuleIO *io, void *obj) {
    // Save the setting!
    SBChain *sb = obj;

    RedisModule_SaveUnsigned(io, sb->size);
    RedisModule_SaveUnsigned(io, sb->nfilters);

    for (size_t ii = 0; ii < sb->nfilters; ++ii) {
        const SBLink *lb = sb->filters + ii;
        const struct bloom *bm = &lb->inner;

        RedisModule_SaveUnsigned(io, bm->entries);
        RedisModule_SaveDouble(io, bm->error);
        // - SKIP: bits is (double)entries * bpe
        RedisModule_SaveUnsigned(io, bm->hashes);
        RedisModule_SaveDouble(io, bm->bpe);
        RedisModule_SaveStringBuffer(io, (const char *)bm->bf, bm->bytes);

        // Save the number of actual entries stored thus far.
        RedisModule_SaveUnsigned(io, lb->size);
    }
}

static void *BFRdbLoad(RedisModuleIO *io, int encver) {
    if (encver != 0) {
        return NULL;
    }

    // Load our modules
    SBChain *sb = RedisModule_Calloc(1, sizeof(*sb));
    sb->size = RedisModule_LoadUnsigned(io);
    sb->nfilters = RedisModule_LoadUnsigned(io);

    // Sanity:
    assert(sb->nfilters < 1000);
    sb->filters = RedisModule_Calloc(sb->nfilters, sizeof(*sb->filters));

    for (size_t ii = 0; ii < sb->nfilters; ++ii) {
        SBLink *lb = sb->filters + ii;
        struct bloom *bm = &lb->inner;

        bm->entries = RedisModule_LoadUnsigned(io);
        bm->error = RedisModule_LoadDouble(io);
        bm->hashes = RedisModule_LoadUnsigned(io);
        bm->bpe = RedisModule_LoadDouble(io);
        bm->bits = (double)bm->entries * bm->bpe;
        size_t sztmp;
        bm->bf = (unsigned char *)RedisModule_LoadStringBuffer(io, &sztmp);
        bm->bytes = sztmp;
        lb->size = RedisModule_LoadUnsigned(io);
    }

    return sb;
}

static void BFAofRewrite(RedisModuleIO *aof, RedisModuleString *key, void *value) {
    RedisModuleCallReply *rep =
        RedisModule_Call(RedisModule_GetContextFromIO(aof), "DUMP", "%s", key);
    if (rep != NULL && RedisModule_CallReplyType(rep) == REDISMODULE_REPLY_STRING) {
        size_t n;
        const char *s = RedisModule_CallReplyStringPtr(rep, &n);
        RedisModule_EmitAOF(aof, "RESTORE", "%sb", key, s, n);
    } else {
        RedisModule_Log(RedisModule_GetContextFromIO(aof), "warning", "Failed to emit AOF");
    }
    if (rep != NULL) {
        RedisModule_FreeCallReply(rep);
    }
}

static void BFFree(void *value) { SBChain_Free(value); }

static size_t BFMemUsage(const void *value) {
    const SBChain *sb = value;
    size_t rv = sizeof(*sb);
    for (size_t ii = 0; ii < sb->nfilters; ++ii) {
        rv += sizeof(*sb->filters);
        rv += sb->filters[ii].inner.bytes;
    }
    return rv;
}

static int rsStrcasecmp(const RedisModuleString *rs1, const char *s2) {
    size_t n1 = strlen(s2);
    size_t n2;
    const char *s1 = RedisModule_StringPtrLen(rs1, &n2);
    if (n1 != n2) {
        return -1;
    }
    return strncasecmp(s1, s2, n1);
}

#define BAIL(s, ...)                                                                               \
    do {                                                                                           \
        RedisModule_Log(ctx, "warning", s, ##__VA_ARGS__);                                         \
        return REDISMODULE_ERR;                                                                    \
    } while (0);

int RedisModule_OnLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (RedisModule_Init(ctx, "bf", 1, REDISMODULE_APIVER_1) != REDISMODULE_OK) {
        return REDISMODULE_ERR;
    }

    if (argc % 2) {
        BAIL("Invalid number of arguments passed");
    }

    for (int ii = 0; ii < argc; ii += 2) {
        if (!rsStrcasecmp(argv[ii], "initial_size")) {
            long long v;
            if (RedisModule_StringToLongLong(argv[ii + 1], &v) == REDISMODULE_ERR) {
                BAIL("Invalid argument for 'INITIAL_SIZE'");
            }
            if (v > 0) {
                BFDefaultInitCapacity = v;
            } else {
                BAIL("INITIAL_SIZE must be > 0");
            }
        } else if (!rsStrcasecmp(argv[ii], "error_rate")) {
            double d;
            if (RedisModule_StringToDouble(argv[ii + 1], &d) == REDISMODULE_ERR) {
                BAIL("Invalid argument for 'ERROR_RATE'");
            } else if (d <= 0) {
                BAIL("ERROR_RATE must be > 0");
            } else {
                BFDefaultErrorRate = d;
            }
        } else {
            BAIL("Unrecognized option");
        }
    }

    if (RedisModule_CreateCommand(ctx, "BF.RESERVE", BFReserve_RedisCommand, "write", 1, 1, 1) !=
        REDISMODULE_OK)
        return REDISMODULE_ERR;
    if (RedisModule_CreateCommand(ctx, "BF.SET", BFAdd_RedisCommand, "write", 1, 1, 1) !=
        REDISMODULE_OK)
        return REDISMODULE_ERR;
    if (RedisModule_CreateCommand(ctx, "BF.TEST", BFCheck_RedisCommand, "readonly", 1, 1, 1) !=
        REDISMODULE_OK)
        return REDISMODULE_ERR;
    if (RedisModule_CreateCommand(ctx, "BF.DEBUG", BFInfo_RedisCommand, "readonly", 1, 1, 1) !=
        REDISMODULE_OK)
        return REDISMODULE_ERR;

    static RedisModuleTypeMethods typeprocs = {.version = REDISMODULE_TYPE_METHOD_VERSION,
                                               .rdb_load = BFRdbLoad,
                                               .rdb_save = BFRdbSave,
                                               .aof_rewrite = BFAofRewrite,
                                               .free = BFFree,
                                               .mem_usage = BFMemUsage};
    BFType = RedisModule_CreateDataType(ctx, "MBbloom--", 0, &typeprocs);
    return BFType == NULL ? REDISMODULE_ERR : REDISMODULE_OK;
}