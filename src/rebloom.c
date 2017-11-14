#include "redismodule.h"
#include "sb.h"
#include "version.h"

#include <assert.h>
#include <strings.h> // strncasecmp
#include <string.h>


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// Redis Commands                                                           ///
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
static RedisModuleType *BFType;
static double BFDefaultErrorRate = 0.01;
static size_t BFDefaultInitCapacity = 100;
static int rsStrcasecmp(const RedisModuleString *rs1, const char *s2);

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
        case SB_EMPTY:
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
    SBChain *sb = SB_NewChain(capacity, error_rate, 0);
    if (sb != NULL) {
        RedisModule_ModuleTypeSetValue(key, BFType, sb);
    }
    return sb;
}

/**
 * Reserves a new empty filter with custom parameters:
 * BF.CREATE <KEY> <ERROR_RATE (double)> <INITIAL_CAPACITY (int)>
 */
static int BFReserve_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    RedisModule_ReplicateVerbatim(ctx);

    if (argc != 4) {
        RedisModule_WrongArity(ctx);
        return REDISMODULE_ERR;
    }

    double error_rate;
    if (RedisModule_StringToDouble(argv[2], &error_rate) != REDISMODULE_OK) {
        return RedisModule_ReplyWithError(ctx, "ERR bad error rate");
    }

    long long capacity;
    if (RedisModule_StringToLongLong(argv[3], &capacity) != REDISMODULE_OK ||
        capacity >= UINT32_MAX) {
        return RedisModule_ReplyWithError(ctx, "ERR bad capacity");
    }

    if (error_rate == 0 || capacity == 0) {
        return RedisModule_ReplyWithError(ctx, "ERR capacity and error must not be 0");
    }

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
    SBChain *sb;
    int status = bfGetChain(key, &sb);
    if (status != SB_EMPTY) {
        return RedisModule_ReplyWithError(ctx, statusStrerror(status));
    }

    if (bfCreateChain(key, error_rate, capacity) == NULL) {
        RedisModule_ReplyWithSimpleString(ctx, "ERR could not create filter");
    } else {
        RedisModule_ReplyWithSimpleString(ctx, "OK");
    }
    return REDISMODULE_OK;
}

static int isMulti(const RedisModuleString *rs) {
    size_t n;
    const char *s = RedisModule_StringPtrLen(rs, &n);
    return s[3] == 'm' || s[3] == 'M';
}

/**
 * Check for the existence of an item
 * BF.TEST <KEY>
 * Returns true or false
 */
static int BFCheck_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    int is_multi = isMulti(argv[0]);

    if ((is_multi == 0 && argc != 3) || (is_multi && argc < 3)) {
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
    if (is_multi) {
        RedisModule_ReplyWithArray(ctx, argc - 2);
    }

    for (size_t ii = 2; ii < argc; ++ii) {
        size_t n;
        const char *s = RedisModule_StringPtrLen(argv[ii], &n);
        int exists = SBChain_Check(sb, s, n);
        RedisModule_ReplyWithLongLong(ctx, exists);
    }

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
    RedisModule_ReplicateVerbatim(ctx);

    int is_multi = isMulti(argv[0]);

    if ((is_multi && argc < 3) || (is_multi == 0 && argc != 3)) {
        RedisModule_WrongArity(ctx);
        return REDISMODULE_ERR;
    }

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
    SBChain *sb;
    int status = bfGetChain(key, &sb);
    if (status == SB_EMPTY) {
        sb = bfCreateChain(key, BFDefaultErrorRate, BFDefaultInitCapacity);
        if (sb == NULL) {
            return RedisModule_ReplyWithError(ctx, "ERR could not create filter");
        }
    } else if (status != SB_OK) {
        return RedisModule_ReplyWithError(ctx, statusStrerror(status));
    }

    if (is_multi) {
        RedisModule_ReplyWithArray(ctx, argc - 2);
    }

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
            ctx, "bytes:%llu bits:%llu hashes:%u capacity:%u size:%lu ratio:%g", lb->inner.bytes,
            lb->inner.bits ? lb->inner.bits : 1LLU << lb->inner.n2, lb->inner.hashes,
            lb->inner.entries, lb->size, lb->inner.error);
        RedisModule_ReplyWithString(ctx, info_s);
        RedisModule_FreeString(ctx, info_s);
    }

    return REDISMODULE_OK;
}

#define MAX_SCANDUMP_SIZE 10485760 // 10MB

/**
 * BF.SCANDUMP <KEY> <ITER>
 * Returns an (iterator,data) pair which can be used for LOADCHUNK later on
 */
static int BFScanDump_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    if (argc != 3) {
        return RedisModule_WrongArity(ctx);
    }
    const SBChain *sb = NULL;
    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
    int status = bfGetChain(key, (SBChain **)&sb);
    if (status != SB_OK) {
        return RedisModule_ReplyWithError(ctx, statusStrerror(status));
    }

    long long iter;
    if (RedisModule_StringToLongLong(argv[2], &iter) != REDISMODULE_OK) {
        return RedisModule_ReplyWithError(ctx, "Second argument must be numeric");
    }

    RedisModule_ReplyWithArray(ctx, 2);

    if (iter == 0) {
        size_t hdrlen;
        char *hdr = SBChain_GetEncodedHeader(sb, &hdrlen);
        RedisModule_ReplyWithLongLong(ctx, SB_CHUNKITER_INIT);
        RedisModule_ReplyWithStringBuffer(ctx, (const char *)hdr, hdrlen);
        SB_FreeEncodedHeader(hdr);
    } else {
        size_t bufLen = 0;
        const char *buf = SBChain_GetEncodedChunk(sb, &iter, &bufLen, MAX_SCANDUMP_SIZE);
        RedisModule_ReplyWithLongLong(ctx, iter);
        RedisModule_ReplyWithStringBuffer(ctx, buf, bufLen);
    }
    return REDISMODULE_OK;
}

/**
 * BF.LOADCHUNK <KEY> <ITER> <DATA>
 * Incrementally loads a bloom filter.
 */
static int BFLoadChunk_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    RedisModule_ReplicateVerbatim(ctx);

    if (argc != 4) {
        return RedisModule_WrongArity(ctx);
    }

    long long iter;
    if (RedisModule_StringToLongLong(argv[2], &iter) != REDISMODULE_OK) {
        return RedisModule_ReplyWithError(ctx, "ERR Second argument must be numeric");
    }

    size_t bufLen;
    const char *buf = RedisModule_StringPtrLen(argv[3], &bufLen);

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
    SBChain *sb;
    int status = bfGetChain(key, &sb);
    if (status == SB_EMPTY && iter == 1) {
        const char *errmsg;
        SBChain *sb = SB_NewChainFromHeader(buf, bufLen, &errmsg);
        if (!sb) {
            return RedisModule_ReplyWithError(ctx, errmsg);
        } else {
            RedisModule_ModuleTypeSetValue(key, BFType, sb);
            return RedisModule_ReplyWithSimpleString(ctx, "OK");
        }
    } else if (status != SB_OK) {
        return RedisModule_ReplyWithError(ctx, statusStrerror(status));
    }

    assert(sb);

    const char *errMsg;
    if (SBChain_LoadEncodedChunk(sb, iter, buf, bufLen, &errMsg) != 0) {
        return RedisModule_ReplyWithError(ctx, errMsg);
    } else {
        return RedisModule_ReplyWithSimpleString(ctx, "OK");
    }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// Datatype Functions                                                       ///
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
#define BF_ENCODING_VERSION 1

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
        RedisModule_SaveUnsigned(io, bm->hashes);
        RedisModule_SaveDouble(io, bm->bpe);
        RedisModule_SaveUnsigned(io, bm->bits);
        RedisModule_SaveUnsigned(io, bm->n2);
        RedisModule_SaveStringBuffer(io, (const char *)bm->bf, bm->bytes);

        // Save the number of actual entries stored thus far.
        RedisModule_SaveUnsigned(io, lb->size);
    }
}

static void *BFRdbLoad(RedisModuleIO *io, int encver) {
    if (encver > BF_ENCODING_VERSION) {
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
        if (encver == 0) {
            bm->bits = (double)bm->entries * bm->bpe;
        } else {
            bm->bits = RedisModule_LoadUnsigned(io);
            bm->n2 = RedisModule_LoadUnsigned(io);
        }
        size_t sztmp;
        bm->bf = (unsigned char *)RedisModule_LoadStringBuffer(io, &sztmp);
        bm->bytes = sztmp;
        lb->size = RedisModule_LoadUnsigned(io);
    }

    return sb;
}

static void BFAofRewrite(RedisModuleIO *aof, RedisModuleString *key, void *value) {
    SBChain *sb = value;
    size_t len;
    char *hdr = SBChain_GetEncodedHeader(sb, &len);
    RedisModule_EmitAOF(aof, "BF.LOADCHUNK", "slb", key, 0, hdr, len);
    SB_FreeEncodedHeader(hdr);

    long long iter = SB_CHUNKITER_INIT;
    const char *chunk;
    while ((chunk = SBChain_GetEncodedChunk(sb, &iter, &len, MAX_SCANDUMP_SIZE)) != NULL) {
        RedisModule_EmitAOF(aof, "BF.LOADCHUNK", "slb", key, iter, chunk, len);
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
    if (RedisModule_Init(ctx, "bf", REBLOOM_MODULE_VERSION, REDISMODULE_APIVER_1) != REDISMODULE_OK) {
        return REDISMODULE_ERR;
    }

    if (argc == 1) {
        RedisModule_Log(ctx, "notice", "Found empty string. Assuming ramp-packer validation");
        // Hack for ramp-packer which gives us an empty string.
        size_t tmp;
        RedisModule_StringPtrLen(argv[0], &tmp);
        if (tmp == 0) {
            argc = 0;
        }
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

    if (RedisModule_CreateCommand(ctx, "BF.RESERVE", BFReserve_RedisCommand, "write deny-oom", 1, 1,
                                  1) != REDISMODULE_OK)
        return REDISMODULE_ERR;
    if (RedisModule_CreateCommand(ctx, "BF.ADD", BFAdd_RedisCommand, "write deny-oom", 1, 1, 1) !=
        REDISMODULE_OK)
        return REDISMODULE_ERR;
    if (RedisModule_CreateCommand(ctx, "BF.MADD", BFAdd_RedisCommand, "write deny-oom", 1, 1, 1) !=
        REDISMODULE_OK)
        return REDISMODULE_ERR;
    if (RedisModule_CreateCommand(ctx, "BF.EXISTS", BFCheck_RedisCommand, "readonly fast", 1, 1,
                                  1) != REDISMODULE_OK)
        return REDISMODULE_ERR;
    if (RedisModule_CreateCommand(ctx, "BF.MEXISTS", BFCheck_RedisCommand, "readonly fast", 1, 1,
                                  1) != REDISMODULE_OK)
        return REDISMODULE_ERR;
    if (RedisModule_CreateCommand(ctx, "BF.DEBUG", BFInfo_RedisCommand, "readonly fast", 1, 1, 1) !=
        REDISMODULE_OK)
        return REDISMODULE_ERR;

    if (RedisModule_CreateCommand(ctx, "BF.SCANDUMP", BFScanDump_RedisCommand, "readonly fast", 1,
                                  1, 1) != REDISMODULE_OK)
        return REDISMODULE_ERR;

    if (RedisModule_CreateCommand(ctx, "BF.LOADCHUNK", BFLoadChunk_RedisCommand, "write deny-oom",
                                  1, 1, 1) != REDISMODULE_OK)
        return REDISMODULE_ERR;

    static RedisModuleTypeMethods typeprocs = {.version = REDISMODULE_TYPE_METHOD_VERSION,
                                               .rdb_load = BFRdbLoad,
                                               .rdb_save = BFRdbSave,
                                               .aof_rewrite = BFAofRewrite,
                                               .free = BFFree,
                                               .mem_usage = BFMemUsage};
    BFType = RedisModule_CreateDataType(ctx, "MBbloom--", BF_ENCODING_VERSION, &typeprocs);
    return BFType == NULL ? REDISMODULE_ERR : REDISMODULE_OK;
}
