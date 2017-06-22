#include "rebloom.h"
#include "redismodule.h"
#define BLOOM_CALLOC RedisModule_Calloc
#define BLOOM_FREE RedisModule_Free
#include "contrib/bloom.c"
#include <string.h>

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// Core                                                                     ///
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
static sbLink *sbCreateLink(size_t size, double error_rate) {
    sbLink *lb = RedisModule_Calloc(1, sizeof(*lb));
    bloom_init(&lb->inner, size, error_rate);
    return lb;
}

void sbFreeChain(sbChain *sb) {
    sbLink *lb = sb->cur;
    while (lb) {
        sbLink *lb_next = lb->next;
        bloom_free(&lb->inner);
        RedisModule_Free(lb);
        lb = lb_next;
    }
    RedisModule_Free(sb);
}

static int sbAddToLink(sbLink *lb, const void *data, size_t len) {
    int newbits = bloom_add_retbits(&lb->inner, data, len);
    lb->fillbits += newbits;
    return newbits;
}

int sbAdd(sbChain *sb, const void *data, size_t len) {
    // Does it already exist?

    if (sbCheck(sb, data, len)) {
        return 1;
    }

    // Determine if we need to add more items?
    if (sb->cur->fillbits * 2 > sb->cur->inner.bits) {
        sbLink *new_lb = sbCreateLink(sb->cur->inner.entries * 2, sb->error);
        new_lb->next = sb->cur;
        sb->cur = new_lb;
    }
    int rv = sbAddToLink(sb->cur, data, len);
    if (rv) {
        sb->total_entries++;
    }
    return rv;
}

int sbCheck(const sbChain *sb, const void *data, size_t len) {
    for (const sbLink *lb = sb->cur; lb; lb = lb->next) {
        if (bloom_check(&lb->inner, data, len)) {
            return 1;
        }
    }
    return 0;
}

sbChain *sbCreateChain(size_t initsize, double error_rate) {
    if (initsize == 0 || error_rate == 0) {
        return NULL;
    }
    sbChain *sb = RedisModule_Calloc(1, sizeof(*sb));
    sb->error = error_rate;
    sb->cur = sbCreateLink(initsize, error_rate);
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

static int bfGetChain(RedisModuleKey *key, sbChain **sbout) {
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
        return "ERR mismatched type";
    case SB_OK:
        return "ERR item exists";
    default:
        return "Unknown error";
    }
}

static int returnWithError(RedisModuleCtx *ctx, const char *errmsg) {
    RedisModule_ReplyWithError(ctx, errmsg);
    return REDISMODULE_ERR;
}

/**
 * Common function for adding one or more items to a bloom filter.
 * @param key the key key associated with the filter
 * @param sb the actual bloom filter
 * @param is_fixed - for creating only, whether this filter is expected to
 *        be fixed
 * @param error_rate error rate for new filter
 * @param elems list of elements to add
 * @param nelems number of elements to add
 */
static void bfAddCommon(RedisModuleKey *key, sbChain *sb, int is_fixed, double error_rate,
                        RedisModuleString **elems, int nelems) {
    if (sb == NULL) {
        if (!error_rate) {
            error_rate = BFDefaultErrorRate;
        }
        size_t capacity = nelems;
        if (!is_fixed && capacity < BFDefaultInitCapacity) {
            capacity = BFDefaultInitCapacity;
        }
        sb = sbCreateChain(capacity, error_rate);
        RedisModule_ModuleTypeSetValue(key, BFType, sb);
        sb->is_fixed = is_fixed;
    }
    // Now, just add the items
    for (size_t ii = 0; ii < nelems; ++ii) {
        size_t n;
        const char *s = RedisModule_StringPtrLen(elems[ii], &n);
        sbAdd(sb, s, n);
    }
}

static int BFCreate_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc < 3) {
        // CMD, ERR, K1
        RedisModule_WrongArity(ctx);
        return REDISMODULE_ERR;
    }

    double error_rate;
    if (RedisModule_StringToDouble(argv[2], &error_rate) != REDISMODULE_OK) {
        return returnWithError(ctx, "ERR error rate required");
    }

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
    sbChain *sb;
    int status = bfGetChain(key, &sb);
    if (status != SB_EMPTY) {
        return returnWithError(ctx, statusStrerror(status));
    }

    bfAddCommon(key, NULL, 1, error_rate, argv + 3, argc - 3);
    RedisModule_ReplyWithNull(ctx);
    return REDISMODULE_OK;
}

static int BFCheck_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc != 3) {
        RedisModule_WrongArity(ctx);
        return REDISMODULE_ERR;
    }

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
    sbChain *sb;
    int status = bfGetChain(key, &sb);
    if (status != SB_OK) {
        return returnWithError(ctx, statusStrerror(status));
    }

    // Check if it exists?
    size_t n;
    const char *s = RedisModule_StringPtrLen(argv[2], &n);
    int exists = sbCheck(sb, s, n);
    RedisModule_ReplyWithLongLong(ctx, exists);
    return REDISMODULE_OK;
}

static int BFAdd_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc < 3) {
        RedisModule_WrongArity(ctx);
        return REDISMODULE_ERR;
    }

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
    sbChain *sb;
    int status = bfGetChain(key, &sb);
    if (status == SB_OK) {
        if (sb->is_fixed) {
            return returnWithError(ctx, "ERR cannot add: filter is fixed");
        }
        size_t namelen;
        const char *cmdname = RedisModule_StringPtrLen(argv[0], &namelen);
        static const char setnxcmd[] = "BF.SETNX";
        if (namelen == sizeof(setnxcmd) - 1 && !strncasecmp(cmdname, "BF.SETNX", namelen)) {
            return returnWithError(ctx, "ERR filter already exists");
        }
    } else if (status != SB_EMPTY) {
        returnWithError(ctx, statusStrerror(status));
    }

    bfAddCommon(key, sb, 0, 0, argv + 2, argc - 2);
    RedisModule_ReplyWithNull(ctx);
    return REDISMODULE_OK;
}

static int BFInfo_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc != 2) {
        RedisModule_WrongArity(ctx);
        return REDISMODULE_ERR;
    }

    const sbChain *sb = NULL;
    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
    int status = bfGetChain(key, (sbChain **)&sb);
    if (status != SB_OK) {
        return returnWithError(ctx, statusStrerror(status));
    }

    // Start writing info
    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);

    // 2
    RedisModule_ReplyWithSimpleString(ctx, "size");
    RedisModule_ReplyWithLongLong(ctx, sb->total_entries);

    // 4
    RedisModule_ReplyWithSimpleString(ctx, "fixed");
    RedisModule_ReplyWithLongLong(ctx, sb->is_fixed);

    // 6
    RedisModule_ReplyWithSimpleString(ctx, "ratio");
    RedisModule_ReplyWithDouble(ctx, sb->error);

    // 7
    RedisModule_ReplyWithSimpleString(ctx, "filters");

    size_t num_elems = 0;

    for (const sbLink *lb = sb->cur; lb; lb = lb->next, num_elems++) {
        RedisModule_ReplyWithArray(ctx, 10);

        // 2
        RedisModule_ReplyWithSimpleString(ctx, "bytes");
        RedisModule_ReplyWithLongLong(ctx, lb->inner.bytes);

        // 4
        RedisModule_ReplyWithSimpleString(ctx, "bits");
        RedisModule_ReplyWithLongLong(ctx, lb->inner.bits);

        // 6
        RedisModule_ReplyWithSimpleString(ctx, "num_filled");
        RedisModule_ReplyWithLongLong(ctx, lb->fillbits);

        // 8
        RedisModule_ReplyWithSimpleString(ctx, "hashes");
        RedisModule_ReplyWithLongLong(ctx, lb->inner.hashes);

        // 10
        RedisModule_ReplyWithSimpleString(ctx, "capacity");
        RedisModule_ReplyWithLongLong(ctx, lb->inner.entries);
    }

    RedisModule_ReplySetArrayLength(ctx, 7 + num_elems);
    return REDISMODULE_OK;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// Datatype Functions                                                       ///
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
static void BFRdbSave(RedisModuleIO *io, void *obj) {
    // Save the setting!
    sbChain *sb = obj;
    // We don't know how many links are here thus far, so
    RedisModule_SaveUnsigned(io, sb->total_entries);
    RedisModule_SaveDouble(io, sb->error);
    RedisModule_SaveUnsigned(io, sb->is_fixed);
    for (const sbLink *lb = sb->cur; lb; lb = lb->next) {
        const struct bloom *bm = &lb->inner;
        RedisModule_SaveUnsigned(io, bm->entries);
        // - SKIP: error ratio is fixed, and stored as part of the header
        // - SKIP: bits is (double)entries * bpe
        RedisModule_SaveUnsigned(io, bm->hashes);
        RedisModule_SaveDouble(io, bm->bpe);
        RedisModule_SaveStringBuffer(io, (const char *)bm->bf, bm->bytes);

        // Save the number of actual entries stored thus far.
        RedisModule_SaveUnsigned(io, lb->fillbits);
    }

    // Finally, save the last 0 indicating that nothing more follows:
    RedisModule_SaveUnsigned(io, 0);
}

static void *BFRdbLoad(RedisModuleIO *io, int encver) {
    if (encver != 0) {
        return NULL;
    }

    // Load our modules
    sbChain *sb = RedisModule_Calloc(1, sizeof(*sb));
    sb->total_entries = RedisModule_LoadUnsigned(io);
    sb->error = RedisModule_LoadDouble(io);
    sb->is_fixed = RedisModule_LoadUnsigned(io);

    // Now load the individual nodes
    sbLink *last = NULL;
    while (1) {
        unsigned entries = RedisModule_LoadUnsigned(io);
        if (!entries) {
            break;
        }
        sbLink *lb = RedisModule_Calloc(1, sizeof(*lb));
        struct bloom *bm = &lb->inner;

        bm->entries = entries;
        bm->error = sb->error;
        bm->hashes = RedisModule_LoadUnsigned(io);
        bm->bpe = RedisModule_LoadDouble(io);
        bm->bits = (double)bm->entries * bm->bpe;
        size_t sztmp;
        bm->bf = (unsigned char *)RedisModule_LoadStringBuffer(io, &sztmp);
        bm->bytes = sztmp;
        bm->ready = 1;
        lb->fillbits = RedisModule_LoadUnsigned(io);
        if (last) {
            last->next = lb;
        } else {
            assert(sb->cur == NULL);
            sb->cur = lb;
        }

        last = lb;
    }

    return sb;
}

static void BFAofRewrite(RedisModuleIO *aof, RedisModuleString *key, void *value) {
    // TODO
    (void)aof;
    (void)key;
    (void)value;
}

static void BFFree(void *value) { sbFreeChain(value); }

static size_t BFMemUsage(const void *value) {
    const sbChain *sb = value;
    size_t rv = sizeof(*sb);
    for (const sbLink *lb = sb->cur; lb; lb = lb->next) {
        rv += sizeof(*lb);
        rv += lb->inner.bytes;
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

    if (RedisModule_CreateCommand(ctx, "BF.CREATE", BFCreate_RedisCommand, "write", 1, 1, 1) !=
        REDISMODULE_OK)
        return REDISMODULE_ERR;
    if (RedisModule_CreateCommand(ctx, "BF.SET", BFAdd_RedisCommand, "write", 1, 1, 1) !=
        REDISMODULE_OK)
        return REDISMODULE_ERR;
    if (RedisModule_CreateCommand(ctx, "BF.SETNX", BFAdd_RedisCommand, "write", 1, 1, 1) !=
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