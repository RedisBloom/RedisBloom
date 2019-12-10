#include "redismodule.h"
#include "sb.h"
#include "cf.h"
#include "rm_cms.h"
#include "rm_topk.h"
#include "version.h"
#include "rmutil/util.h"

#include <assert.h>
#include <strings.h> // strncasecmp
#include <string.h>
#include <ctype.h>

#define CF_MAX_ITERATIONS 20
#define CF_DEFAULT_BUCKETSIZE 2
#define CF_DEFAULT_EXPANSION 1
#define BF_DEFAULT_EXPANSION 2

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// Redis Commands                                                           ///
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
static RedisModuleType *BFType;
static RedisModuleType *CFType;
static double BFDefaultErrorRate = 0.01;
static size_t BFDefaultInitCapacity = 100;
static size_t CFDefaultInitCapacity = 1000;
static size_t CFMaxExpansions = 32;
static int rsStrcasecmp(const RedisModuleString *rs1, const char *s2);

typedef enum { SB_OK = 0, SB_MISSING, SB_EMPTY, SB_MISMATCH } lookupStatus;

typedef struct {
    long long capacity;
    double error_rate;
    int autocreate;
    // int must_exist;
    int is_multi;
    long long expansion;
    long long nonScaling;
} BFInsertOptions;

static int getValue(RedisModuleKey *key, RedisModuleType *expType, void **sbout) {
    *sbout = NULL;
    if (key == NULL) {
        return SB_MISSING;
    }
    int type = RedisModule_KeyType(key);
    if (type == REDISMODULE_KEYTYPE_EMPTY) {
        return SB_EMPTY;
    } else if (type == REDISMODULE_KEYTYPE_MODULE &&
               RedisModule_ModuleTypeGetType(key) == expType) {
        *sbout = RedisModule_ModuleTypeGetValue(key);
        return SB_OK;
    } else {
        return SB_MISMATCH;
    }
}

static int bfGetChain(RedisModuleKey *key, SBChain **sbout) {
    return getValue(key, BFType, (void **)sbout);
}

static int cfGetFilter(RedisModuleKey *key, CuckooFilter **cfout) {
    return getValue(key, CFType, (void **)cfout);
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
    default: // LCOV_EXCL_LINE
        return "Unknown error";
    }
}

/**
 * Common function for adding one or more items to a bloom filter.
 * capacity and error rate must not be 0.
 */
static SBChain *bfCreateChain(RedisModuleKey *key, double error_rate,
                              size_t capacity, unsigned expansion, unsigned scaling) {
    SBChain *sb = SB_NewChain(capacity, error_rate, BLOOM_OPT_FORCE64 | scaling | BLOOM_OPT_NOROUND, expansion);
    if (sb != NULL) {
        RedisModule_ModuleTypeSetValue(key, BFType, sb);
    }
    return sb;
}

static CuckooFilter *cfCreate(RedisModuleKey *key, size_t capacity,
                        size_t bucketSize, size_t maxIterations, size_t expansion) {
    if (capacity < bucketSize * 2) return NULL;
    
    CuckooFilter *cf = RedisModule_Calloc(1, sizeof(*cf));
    if (CuckooFilter_Init(cf, capacity, bucketSize, maxIterations, expansion) != 0) {
        RedisModule_Free(cf); // LCOV_EXCL_LINE
        cf = NULL; // LCOV_EXCL_LINE
    }
    RedisModule_ModuleTypeSetValue(key, CFType, cf);
    return cf;
}

/**
 * Reserves a new empty filter with custom parameters:
 * BF.RESERVE <KEY> <ERROR_RATE (double)> <INITIAL_CAPACITY (int)> [NONSCALING]
 */
static int BFReserve_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc < 4 || argc > 7) {
        return RedisModule_WrongArity(ctx);
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

    long long expansion = BF_DEFAULT_EXPANSION;
    int ex_loc = RMUtil_ArgIndex("EXPANSION", argv, argc);
    if (ex_loc + 1 == argc) {
        return RedisModule_ReplyWithError(ctx, "ERR no expansion");
    }  
    if (ex_loc != -1) {
        if (RedisModule_StringToLongLong(argv[ex_loc + 1], &expansion) != REDISMODULE_OK) {
            return RedisModule_ReplyWithError(ctx, "ERR bad expansion");
        }
    }

    unsigned nonScaling = 0;
    ex_loc = RMUtil_ArgIndex("NONSCALING", argv, argc);    
    if (ex_loc != -1) {
        nonScaling = BLOOM_OPT_NO_SCALING;
    }

    if (error_rate == 0 || capacity == 0) {
        return RedisModule_ReplyWithError(ctx, "ERR capacity and error must not be 0");
    } else if (expansion < 1) {
        return RedisModule_ReplyWithError(ctx, "ERR expansion must be great than 0");
    }

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
    SBChain *sb;
    int status = bfGetChain(key, &sb);
    if (status != SB_EMPTY) {
        return RedisModule_ReplyWithError(ctx, statusStrerror(status));
    }

    if (bfCreateChain(key, error_rate, capacity, expansion, nonScaling) == NULL) {
        RedisModule_ReplyWithError(ctx, "ERR could not create filter"); // LCOV_EXCL_LINE
    } else {
        RedisModule_ReplyWithSimpleString(ctx, "OK");
    }
    RedisModule_ReplicateVerbatim(ctx);
    return REDISMODULE_OK;
}

static int isMulti(const RedisModuleString *rs) {
    size_t n;
    const char *s = RedisModule_StringPtrLen(rs, &n);
    return s[3] == 'm' || s[3] == 'M';
}

/**
 * Check for the existence of an item
 * BF.CHECK <KEY>
 * Returns true or false
 */
static int BFCheck_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    int is_multi = isMulti(argv[0]);

    if ((is_multi == 0 && argc != 3) || (is_multi && argc < 3)) {
        return RedisModule_WrongArity(ctx);
    }

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
    SBChain *sb;
    int status = bfGetChain(key, &sb);

    int is_empty = 0;
    if (status != SB_OK) {
        is_empty = 1;
    }

    // Check if it exists?
    if (is_multi) {
        RedisModule_ReplyWithArray(ctx, argc - 2);
    }

    for (size_t ii = 2; ii < argc; ++ii) {
        if (is_empty == 1) {
            RedisModule_ReplyWithLongLong(ctx, 0);
        } else {
            size_t n;
            const char *s = RedisModule_StringPtrLen(argv[ii], &n);
            int exists = SBChain_Check(sb, s, n);
            RedisModule_ReplyWithLongLong(ctx, exists);
        }
    }

    return REDISMODULE_OK;
}

static int bfInsertCommon(RedisModuleCtx *ctx, RedisModuleString *keystr, RedisModuleString **items,
                          size_t nitems, const BFInsertOptions *options) {
    RedisModuleKey *key = RedisModule_OpenKey(ctx, keystr, REDISMODULE_READ | REDISMODULE_WRITE);
    SBChain *sb;
    int status = bfGetChain(key, &sb);
    if (status == SB_EMPTY && options->autocreate) {
        sb = bfCreateChain(key, options->error_rate, options->capacity, options->expansion, options->nonScaling);
        if (sb == NULL) {
            return RedisModule_ReplyWithError(ctx, "ERR could not create filter"); // LCOV_EXCL_LINE
        }
    } else if (status != SB_OK) {
        return RedisModule_ReplyWithError(ctx, statusStrerror(status));
    }

    if (options->is_multi) {
        RedisModule_ReplyWithArray(ctx, nitems);
    }

    for (size_t ii = 0; ii < nitems; ++ii) {
        size_t n;
        const char *s = RedisModule_StringPtrLen(items[ii], &n);
        int rv = SBChain_Add(sb, s, n);
        if (rv == -2) { // decide if to make into an error
            return RedisModule_ReplyWithError(ctx, "Non scaling filter is full");
        }
        RedisModule_ReplyWithLongLong(ctx, !!rv);
    }
    RedisModule_ReplicateVerbatim(ctx);
    return REDISMODULE_OK;
}

/**
 * Adds items to an existing filter. Creates a new one on demand if it doesn't exist.
 * BF.ADD <KEY> ITEMS...
 * Returns an array of integers. The nth element is either 1 or 0 depending on whether it was newly
 * added, or had previously existed, respectively.
 */
static int BFAdd_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    BFInsertOptions options = {
        .capacity = BFDefaultInitCapacity, .error_rate = BFDefaultErrorRate,
        .autocreate = 1, .expansion = BF_DEFAULT_EXPANSION, .nonScaling = 0};
    options.is_multi = isMulti(argv[0]);

    if ((options.is_multi && argc < 3) || (!options.is_multi && argc != 3)) {
        return RedisModule_WrongArity(ctx);
    }
    return bfInsertCommon(ctx, argv[1], argv + 2, argc - 2, &options);
}

/**
 * BF.INSERT {filter} [ERROR {rate} CAPACITY {cap} EXPANSION {expansion}]
 *                    [NOCREATE] [NONSCALING] ITEMS {item} {item}
 * ..
 * -> (Array) (or error )
 */
static int BFInsert_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    BFInsertOptions options = {.capacity = BFDefaultInitCapacity,
                               .error_rate = BFDefaultErrorRate,
                               .autocreate = 1,
                               .is_multi = 1,
                               .expansion = BF_DEFAULT_EXPANSION,
                               .nonScaling = 0};
    int items_index = -1;

    // Scan the arguments
    if (argc < 4) {
        return RedisModule_WrongArity(ctx);
    }

    size_t cur_pos = 2;
    while (cur_pos < argc && items_index < 0) {
        size_t arglen;
        const char *argstr = RedisModule_StringPtrLen(argv[cur_pos], &arglen);

        switch (tolower(*argstr)) {
        case 'i':
            items_index = ++cur_pos;
            break;

        case 'e':
            if (++cur_pos == argc) {
                return RedisModule_WrongArity(ctx);
            }
            if (tolower(*(argstr + 1)) == 'r') { // error rate
                if (RedisModule_StringToDouble(argv[cur_pos++], &options.error_rate) !=
                    REDISMODULE_OK) {
                    return RedisModule_ReplyWithError(ctx, "Bad error rate");
                }
            } else { // expansion
                if (RedisModule_StringToLongLong(argv[cur_pos++], &options.expansion) !=
                    REDISMODULE_OK) {
                    return RedisModule_ReplyWithError(ctx, "Bad expansion");
                }
            }
            break;

        case 'c':
            if (++cur_pos == argc) {
                return RedisModule_WrongArity(ctx);
            }
            if (RedisModule_StringToLongLong(argv[cur_pos++], &options.capacity) !=
                REDISMODULE_OK) {
                return RedisModule_ReplyWithError(ctx, "Bad capacity");
            }
            break;

        case 'n':
            if (tolower(*(argstr + 2)) == 'c') {
                options.autocreate = 0;
            } else {
                options.nonScaling = BLOOM_OPT_NO_SCALING;
            }
            cur_pos++;
            break;

        default:
            return RedisModule_ReplyWithError(ctx, "Unknown argument received");
        }
    }
    if (items_index < 0 || items_index == argc) {
        return RedisModule_WrongArity(ctx);
    }

    if (options.error_rate <= 0 || options.error_rate >= 1 || options.capacity < 1 || options.expansion < 1) {
        return RedisModule_ReplyWithError(ctx, "Bad argument received");
    }
    return bfInsertCommon(ctx, argv[1], argv + items_index, argc - items_index, &options);
}

/**
 * BF.DEBUG KEY
 * returns some information about the bloom filter.
 */
static int BFDebug_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc != 2) {
        return RedisModule_WrongArity(ctx);
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
            ctx, "bytes:%llu bits:%llu hashes:%u hashwidth:%u capacity:%u size:%lu ratio:%g",
            lb->inner.bytes, lb->inner.bits ? lb->inner.bits : 1LLU << lb->inner.n2,
            lb->inner.hashes, sb->options & BLOOM_OPT_FORCE64 ? 64 : 32, lb->inner.entries,
            lb->size, lb->inner.error);
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
        RedisModule_ReplicateVerbatim(ctx); // Should be replicated?
        return RedisModule_ReplyWithSimpleString(ctx, "OK");
    }
}

/** CF.RESERVE <KEY> <CAPACITY> [BUCKETSIZE] [MAXITERATIONS] [EXPANSION] */
static int CFReserve_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    //
    if (argc != 3 && (argc % 2) == 0) {
        return RedisModule_WrongArity(ctx);
    }

    long long capacity;
    if (RedisModule_StringToLongLong(argv[2], &capacity)) {
        return RedisModule_ReplyWithError(ctx, "Bad capacity");
    }

    long long maxIterations = CF_MAX_ITERATIONS;
    int mi_loc = RMUtil_ArgIndex("MAXITERATIONS", argv, argc);    
    if (mi_loc != -1) {
        if (RedisModule_StringToLongLong(argv[mi_loc + 1], &maxIterations) != REDISMODULE_OK) {
            return RedisModule_ReplyWithError(ctx, "Couldn't parse MAXITERATIONS");
        }
    }

    long long bucketSize = CF_DEFAULT_BUCKETSIZE;
    int bs_loc = RMUtil_ArgIndex("BUCKETSIZE", argv, argc);    
    if (bs_loc != -1) {
        if (RedisModule_StringToLongLong(argv[bs_loc + 1], &bucketSize) != REDISMODULE_OK) {
            return RedisModule_ReplyWithError(ctx, "Couldn't parse BUCKETSIZE");
        }
    }

    long long expansion = CF_DEFAULT_EXPANSION;
    int ex_loc = RMUtil_ArgIndex("EXPANSION", argv, argc);    
    if (ex_loc != -1) {
        if (RedisModule_StringToLongLong(argv[ex_loc + 1], &expansion) != REDISMODULE_OK) {
            return RedisModule_ReplyWithError(ctx, "Couldn't parse EXPANSION");
        }
    }

    if (bucketSize * 2 > capacity) {
        return RedisModule_ReplyWithError(ctx, "Capacity must be at least (BucketSize * 2)");
    }

    CuckooFilter *cf;
    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
    int status = cfGetFilter(key, &cf);
    if (status != SB_EMPTY) {
        return RedisModule_ReplyWithError(ctx, statusStrerror(status));
    }

    cf = cfCreate(key, capacity, bucketSize, maxIterations, expansion);
    if (cf == NULL) {
        return RedisModule_ReplyWithError(ctx, "Couldn't create Cuckoo Filter"); // LCOV_EXCL_LINE
    } else {
        RedisModule_ReplicateVerbatim(ctx);
        return RedisModule_ReplyWithSimpleString(ctx, "OK");
    }
}

typedef struct {
    int is_nx;
    int autocreate;
    int is_multi;
    long long capacity;
} CFInsertOptions;

static int cfInsertCommon(RedisModuleCtx *ctx, RedisModuleString *keystr, RedisModuleString **items,
                          size_t nitems, const CFInsertOptions *options) {
    RedisModuleKey *key = RedisModule_OpenKey(ctx, keystr, REDISMODULE_READ | REDISMODULE_WRITE);
    CuckooFilter *cf = NULL;
    int status = cfGetFilter(key, &cf);

    if (status == SB_EMPTY && options->autocreate) {
        if ((cf = cfCreate(key, options->capacity, CF_DEFAULT_BUCKETSIZE, CF_MAX_ITERATIONS, CF_DEFAULT_EXPANSION)) == NULL) {
            return RedisModule_ReplyWithError(ctx, "Could not create filter"); // LCOV_EXCL_LINE
        }
    } else if (status != SB_OK) {
        return RedisModule_ReplyWithError(ctx, statusStrerror(status));
    }

    if (cf->numFilters >= CFMaxExpansions) {
        // Ensure that adding new elements does not cause heavy expansion.
        // We might want to find a way to better distinguish legitimate from malicious
        // additions.
        return RedisModule_ReplyWithError(ctx, "Maximum expansions reached");
    }

    // See if we can add the element
    if (options->is_multi) {
        RedisModule_ReplyWithArray(ctx, nitems);
    }

    for (size_t ii = 0; ii < nitems; ++ii) {
        size_t elemlen;
        const char *elem = RedisModule_StringPtrLen(items[ii], &elemlen);
        CuckooHash hash = CUCKOO_GEN_HASH(elem, elemlen);
        CuckooInsertStatus insStatus;
        if (options->is_nx) {
            insStatus = CuckooFilter_InsertUnique(cf, hash);
        } else {
            insStatus = CuckooFilter_Insert(cf, hash);
        }
        switch (insStatus)
        {
        case CuckooInsert_Inserted:
            RedisModule_ReplyWithLongLong(ctx, 1);
            break;
        case CuckooInsert_Exists:
            RedisModule_ReplyWithLongLong(ctx, 0);
            break;
        case CuckooInsert_NoSpace:
            if (!options->is_multi) {
                return RedisModule_ReplyWithError(ctx, "Filter is full");
            } else {
                RedisModule_ReplyWithLongLong(ctx, -1);
            }
        case CuckooInsert_MemAllocFailed:
            RedisModule_ReplyWithError(ctx, "Memory allocation failure");// LCOV_EXCL_LINE
            break;
        default:
            break;
        }
    }

    RedisModule_ReplicateVerbatim(ctx);
    return REDISMODULE_OK;
}

/**
 * CF.ADD <KEY> <ELEM>
 *
 * Adds an item to a cuckoo filter, potentially creating a new cuckoo filter
 */
static int CFAdd_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    CFInsertOptions options = {.autocreate = 1, .capacity = CFDefaultInitCapacity, .is_multi = 0};
    size_t cmdlen;
    const char *cmdstr = RedisModule_StringPtrLen(argv[0], &cmdlen);
    options.is_nx = tolower(cmdstr[cmdlen - 1]) == 'x';
    if (argc != 3) {
        return RedisModule_WrongArity(ctx);
    }
    return cfInsertCommon(ctx, argv[1], argv + 2, 1, &options);
}

/**
 * CF.INSERT <KEY> [NOCREATE] [CAPACITY <cap>] ITEMS <item...>
 */
static int CFInsert_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    CFInsertOptions options = {.autocreate = 1, .capacity = CFDefaultInitCapacity, .is_multi = 1};
    size_t cmdlen;
    const char *cmdstr = RedisModule_StringPtrLen(argv[0], &cmdlen);
    options.is_nx = tolower(cmdstr[cmdlen - 1]) == 'x';
    // Need <cmd> <key> <ITEMS> <n..> -- at least 4 arguments
    if (argc < 4) {
        return RedisModule_WrongArity(ctx);
    }

    size_t cur_pos = 2;
    int items_pos = -1;
    while (cur_pos < argc && items_pos < 0) {
        size_t n;
        const char *argstr = RedisModule_StringPtrLen(argv[cur_pos], &n);
        switch (tolower(*argstr)) {
        case 'c':
            if (++cur_pos == argc) {
                return RedisModule_WrongArity(ctx);
            }
            if (RedisModule_StringToLongLong(argv[cur_pos++], &options.capacity) !=
                REDISMODULE_OK) {
                return RedisModule_ReplyWithError(ctx, "Bad capacity");
            }
            break;
        case 'i':
            // Begin item list
            items_pos = ++cur_pos;
            break;
        case 'n':
            options.autocreate = 0;
            cur_pos++;
            break;
        default:
            return RedisModule_ReplyWithError(ctx, "Unknown argument received");
        }
    }

    if (items_pos < 0 || items_pos == argc) {
        return RedisModule_WrongArity(ctx);
    }
    return cfInsertCommon(ctx, argv[1], argv + items_pos, argc - items_pos, &options);
}

static int isCount(RedisModuleString *s) {
    size_t n;
    const char *ss = RedisModule_StringPtrLen(s, &n);
    return toupper(ss[n - 1]) == 'T';
}

/**
 * Copy-paste from BFCheck :'(
 */
static int CFCheck_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    int is_multi = isMulti(argv[0]);
    int is_count = isCount(argv[0]);

    if ((is_multi == 0 && argc != 3) || (is_multi && argc < 3)) {
        return RedisModule_WrongArity(ctx);
    }

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
    CuckooFilter *cf;
    int status = cfGetFilter(key, &cf);

    int is_empty = 0;
    if (status != SB_OK) {
        is_empty = 1;
    }

    // Check if it exists?
    if (is_multi) {
        RedisModule_ReplyWithArray(ctx, argc - 2);
    }

    for (size_t ii = 2; ii < argc; ++ii) {
        if (is_empty == 1) {
            RedisModule_ReplyWithLongLong(ctx, 0);
        } else {
            size_t n;
            const char *s = RedisModule_StringPtrLen(argv[ii], &n);
            CuckooHash hash = CUCKOO_GEN_HASH(s, n);
            long long rv;
            if (is_count) {
                rv = CuckooFilter_Count(cf, hash);
            } else {
                rv = CuckooFilter_Check(cf, hash);
            }
            RedisModule_ReplyWithLongLong(ctx, rv);
        }
    }
    return REDISMODULE_OK;
}

static int CFDel_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc != 3) {
        return RedisModule_WrongArity(ctx);
    }

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
    CuckooFilter *cf;
    int status = cfGetFilter(key, &cf);
    if (status != SB_OK) {
        return RedisModule_ReplyWithError(ctx, "Not found");
    }

    RedisModule_ReplicateVerbatim(ctx);
    
    size_t elemlen;
    const char *elem = RedisModule_StringPtrLen(argv[2], &elemlen);
    CuckooHash hash = CUCKOO_GEN_HASH(elem, elemlen);
    return RedisModule_ReplyWithLongLong(ctx, CuckooFilter_Delete(cf, hash));
}

static int CFCompact_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc != 2) {
        return RedisModule_WrongArity(ctx);
    }

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
    CuckooFilter *cf;
    int status = cfGetFilter(key, &cf);
    if (status != SB_OK) {
        return RedisModule_ReplyWithError(ctx, "Cuckoo filter was not found");
    }
    CuckooFilter_Compact(cf);
    RedisModule_ReplicateVerbatim(ctx);
    return RedisModule_ReplyWithSimpleString(ctx, "OK");
}

static int CFScanDump_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc != 3) {
        return RedisModule_WrongArity(ctx);
    }

    long long pos;
    if (RedisModule_StringToLongLong(argv[2], &pos) != REDISMODULE_OK) {
        return RedisModule_ReplyWithError(ctx, "Invalid position");
    }

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
    CuckooFilter *cf;
    int status = cfGetFilter(key, &cf);
    if (status != SB_OK) {
        return RedisModule_ReplyWithError(ctx, statusStrerror(status));
    }

    RedisModule_ReplyWithArray(ctx, 2);
    if (!cf->numItems) {
        RedisModule_ReplyWithLongLong(ctx, 0);
        RedisModule_ReplyWithNull(ctx);
        return REDISMODULE_OK;
    }

    // Start
    if (pos == 0) {
        CFHeader header;
        fillCFHeader(&header, cf);
        RedisModule_ReplyWithLongLong(ctx, 1);
        RedisModule_ReplyWithStringBuffer(ctx, (const char *)&header, sizeof header);
        return REDISMODULE_OK;
    }

    size_t chunkLen;
    const char *chunk = CF_GetEncodedChunk(cf, &pos, &chunkLen, MAX_SCANDUMP_SIZE);
    if (chunk == NULL) {
        RedisModule_ReplyWithLongLong(ctx, 0);
        RedisModule_ReplyWithNull(ctx);
    } else {
        RedisModule_ReplyWithLongLong(ctx, pos);
        RedisModule_ReplyWithStringBuffer(ctx, chunk, chunkLen);
    }
    return REDISMODULE_OK;
}

static int CFLoadChunk_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    if (argc != 4) {
        return RedisModule_WrongArity(ctx);
    }

    CuckooFilter *cf;
    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
    int status = cfGetFilter(key, &cf);

    // Pos, blob
    long long pos;
    if (RedisModule_StringToLongLong(argv[2], &pos) != REDISMODULE_OK || pos == 0) {
        return RedisModule_ReplyWithError(ctx, "Invalid position");
    }
    size_t bloblen;
    const char *blob = RedisModule_StringPtrLen(argv[3], &bloblen);

    if (pos == 1) {
        if (status != SB_EMPTY) {
            return RedisModule_ReplyWithError(ctx, statusStrerror(status));
        } else if (bloblen != sizeof(CFHeader)) {
            return RedisModule_ReplyWithError(ctx, "Invalid header");
        }

        cf = CFHeader_Load((CFHeader *)blob);
        if (cf == NULL) {
            return RedisModule_ReplyWithError(ctx, "Couldn't create filter!");
        }
        RedisModule_ModuleTypeSetValue(key, CFType, cf);
        return RedisModule_ReplyWithSimpleString(ctx, "OK");
    }

    if (status != SB_OK) {
        return RedisModule_ReplyWithError(ctx, statusStrerror(status));
    }

    if (CF_LoadEncodedChunk(cf, pos, blob, bloblen) != REDISMODULE_OK) {
        return RedisModule_ReplyWithError(ctx, "Couldn't load chunk!");
    }
    return RedisModule_ReplyWithSimpleString(ctx, "OK");
}

uint64_t BFCapacity(SBChain *bf) {
    uint64_t capacity = 0;
    for(size_t ii = 0; ii < bf->nfilters; ++ii) {
        capacity += bf->filters[ii].inner.entries; // * sizeof(unsigned char);
    }
    return capacity;
}

uint64_t BFSize(SBChain *bf) {
    uint64_t bytes = 0;
    for(size_t ii = 0; ii < bf->nfilters; ++ii) {
        bytes += bf->filters[ii].inner.bytes; // * sizeof(unsigned char);
    }

    return  sizeof(*bf) + 
            sizeof(*bf->filters) * bf->nfilters +
            sizeof(struct bloom) * bf->nfilters +
            bytes;
}

static int BFInfo_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    if (argc != 2) {
        return RedisModule_WrongArity(ctx);
    }

    SBChain *bf;
    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
    int status = bfGetChain(key, &bf);
    if (status != REDISMODULE_OK) {
        return RedisModule_ReplyWithError(ctx, statusStrerror(status));
    }

    RedisModule_ReplyWithArray(ctx, 5 * 2);
    RedisModule_ReplyWithSimpleString(ctx, "Capacity");
    RedisModule_ReplyWithLongLong(ctx, BFCapacity(bf));
    RedisModule_ReplyWithSimpleString(ctx, "Size");
    RedisModule_ReplyWithLongLong(ctx, BFSize(bf));
    RedisModule_ReplyWithSimpleString(ctx, "Number of filters");
    RedisModule_ReplyWithLongLong(ctx, bf->nfilters);
    RedisModule_ReplyWithSimpleString(ctx, "Number of items inserted");
    RedisModule_ReplyWithLongLong(ctx, bf->size);
    RedisModule_ReplyWithSimpleString(ctx, "Expansion rate");
    RedisModule_ReplyWithLongLong(ctx, bf->growth);

    return REDISMODULE_OK;
}

uint64_t CFSize(CuckooFilter *cf) {
    uint64_t numBuckets = 0;
    for(uint16_t ii = 0; ii < cf->numFilters; ++ii) {
        numBuckets += cf->filters[ii].numBuckets;
    }

    return  sizeof(*cf) + 
            sizeof(*cf->filters) * cf->numFilters +
            numBuckets * cf->bucketSize;
}

static int CFInfo_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    if (argc != 2) {
        return RedisModule_WrongArity(ctx);
    }

    CuckooFilter *cf;
    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
    int status = cfGetFilter(key, &cf);
    if (status != REDISMODULE_OK) {
        return RedisModule_ReplyWithError(ctx, statusStrerror(status));
    }

    RedisModule_ReplyWithArray(ctx, 8 * 2);
    RedisModule_ReplyWithSimpleString(ctx, "Size");
    RedisModule_ReplyWithLongLong(ctx, CFSize(cf));
    RedisModule_ReplyWithSimpleString(ctx, "Number of buckets");
    RedisModule_ReplyWithLongLong(ctx, cf->numBuckets);
    RedisModule_ReplyWithSimpleString(ctx, "Number of filters");
    RedisModule_ReplyWithLongLong(ctx, cf->numFilters);
    RedisModule_ReplyWithSimpleString(ctx, "Number of items inserted");
    RedisModule_ReplyWithLongLong(ctx, cf->numItems);
    RedisModule_ReplyWithSimpleString(ctx, "Number of items deleted");
    RedisModule_ReplyWithLongLong(ctx, cf->numDeletes);
    RedisModule_ReplyWithSimpleString(ctx, "Bucket size");
    RedisModule_ReplyWithLongLong(ctx, cf->bucketSize);
    RedisModule_ReplyWithSimpleString(ctx, "Expansion rate");
    RedisModule_ReplyWithLongLong(ctx, cf->expansion);
    RedisModule_ReplyWithSimpleString(ctx, "Max iterations");
    RedisModule_ReplyWithLongLong(ctx, cf->maxIterations);

    return REDISMODULE_OK;
}

static int CFDebug_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);
    if (argc != 2) {
        return RedisModule_WrongArity(ctx);
    }

    CuckooFilter *cf;
    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
    int status = cfGetFilter(key, &cf);
    if (status != REDISMODULE_OK) {
        return RedisModule_ReplyWithError(ctx, statusStrerror(status));
    }

    RedisModuleString *resp = RedisModule_CreateStringPrintf(
        ctx, "bktsize:%lu buckets:%lu items:%lu deletes:%lu filters:%lu max_iterations:%lu expansion:%lu",
        cf->bucketSize, cf->numBuckets, cf->numItems, cf->numDeletes, 
        cf->numFilters, cf->maxIterations, cf->expansion);
    return RedisModule_ReplyWithString(ctx, resp);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// Datatype Functions                                                       ///
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
#define BF_MIN_OPTIONS_ENC 2
#define BF_ENCODING_VERSION 3
#define BF_MIN_GROWTH_ENC 4

#define CF_MIN_EXPANSION_VERSION 4

static void BFRdbSave(RedisModuleIO *io, void *obj) {
    // Save the setting!
    SBChain *sb = obj;

    RedisModule_SaveUnsigned(io, sb->size);
    RedisModule_SaveUnsigned(io, sb->nfilters);
    RedisModule_SaveUnsigned(io, sb->options);
    RedisModule_SaveUnsigned(io, sb->growth);

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
    if (encver > BF_MIN_GROWTH_ENC) {
        return NULL;
    }

    // Load our modules
    SBChain *sb = RedisModule_Calloc(1, sizeof(*sb));
    sb->size = RedisModule_LoadUnsigned(io);
    sb->nfilters = RedisModule_LoadUnsigned(io);
    if (encver >= BF_MIN_OPTIONS_ENC) {
        sb->options = RedisModule_LoadUnsigned(io);
    }
    if (encver >= BF_MIN_GROWTH_ENC) {
        sb->growth = RedisModule_LoadUnsigned(io);
    } else {
        sb->growth = 2;
    }
    

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
        if (sb->options & BLOOM_OPT_FORCE64) {
            bm->force64 = 1;
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

static void CFFree(void *value) {
    CuckooFilter_Free(value);
    RedisModule_Free(value);
}

static void CFRdbSave(RedisModuleIO *io, void *obj) {
    CuckooFilter *cf = obj;
    RedisModule_SaveUnsigned(io, cf->numFilters);
    RedisModule_SaveUnsigned(io, cf->numBuckets);
    RedisModule_SaveUnsigned(io, cf->numItems);
    RedisModule_SaveUnsigned(io, cf->numDeletes);
    RedisModule_SaveUnsigned(io, cf->bucketSize);
    RedisModule_SaveUnsigned(io, cf->maxIterations);
    RedisModule_SaveUnsigned(io, cf->expansion);
    for (size_t ii = 0; ii < cf->numFilters; ++ii) {
        RedisModule_SaveUnsigned(io, cf->filters[ii].numBuckets);
        RedisModule_SaveStringBuffer(io, (char *)cf->filters[ii].data,
                    cf->filters[ii].bucketSize *
                    cf->filters[ii].numBuckets * 
                    sizeof(*cf->filters[ii].data));
    }
}

static void *CFRdbLoad(RedisModuleIO *io, int encver) {
    if (encver > CF_MIN_EXPANSION_VERSION) {
        return NULL;
    }
/* RDBCF
    if (encver == BF_ENCODING_VERSION) { // 3
        globalCuckooHash64Bit = 0;
     //   RedisModule_Log(io->ctx, "warning", "RedisBloom Cuckoo filter started with 32 bit hashing. \
                                  This mode will be deprecated in RedisBloom 3.0")
        printf("\n32 bit mode\n\n");
    } else {
        globalCuckooHash64Bit = 1;
      //  RedisModule_Log(io->ctx, "warning", "RedisBloom Cuckoo filter started with 64 bit hashing")
        printf("\n64 bit mode\n\n");
    }*/

    CuckooFilter *cf = RedisModule_Calloc(1, sizeof(*cf));
    cf->numFilters = RedisModule_LoadUnsigned(io);
    cf->numBuckets = RedisModule_LoadUnsigned(io);
    cf->numItems = RedisModule_LoadUnsigned(io);
    if (encver < CF_MIN_EXPANSION_VERSION) {    // CF_ENCODING_VERSION when added
        cf->numDeletes = 0;                     // Didn't exist earlier. bug fix         
        cf->bucketSize = CF_DEFAULT_BUCKETSIZE;
        cf->maxIterations = CF_MAX_ITERATIONS;
        cf->expansion = CF_DEFAULT_EXPANSION;
    } else {
        cf->numDeletes = RedisModule_LoadUnsigned(io);    
        cf->bucketSize = RedisModule_LoadUnsigned(io);
        cf->maxIterations = RedisModule_LoadUnsigned(io);
        cf->expansion = RedisModule_LoadUnsigned(io);
    }

    cf->filters = RedisModule_Calloc(cf->numFilters, sizeof(*cf->filters));
    for (size_t ii = 0, exp = 1; ii < cf->numFilters; ++ii, exp *= cf->expansion) {
        cf->filters[ii].bucketSize = cf->bucketSize;

        if (encver < CF_MIN_EXPANSION_VERSION) {
            cf->filters[ii].numBuckets = cf->numBuckets;
        } else {
            cf->filters[ii].numBuckets = RedisModule_LoadUnsigned(io);
        }
        
        size_t lenDummy = 0;
        cf->filters[ii].data = (MyCuckooBucket *)RedisModule_LoadStringBuffer(io, &lenDummy);
        assert(cf->filters[ii].data != NULL && lenDummy == cf->filters[ii].bucketSize *
                                                           cf->filters[ii].numBuckets * 
                                                           sizeof(*cf->filters[ii].data));
    }
    return cf;
}

static size_t CFMemUsage(const void *value) {
    const CuckooFilter *cf = value;

    size_t filtersSize = 0;
    for (size_t ii = 0; ii < cf->numFilters; ++ii) {
        filtersSize +=  cf->filters[ii].bucketSize * 
                        cf->filters[ii].numBuckets * 
                        sizeof(*cf->filters[ii].data);
    }
    
    return sizeof(*cf) + sizeof(*cf->filters) * cf->numFilters + filtersSize;
}

static void CFAofRewrite(RedisModuleIO *aof, RedisModuleString *key, void *obj) {
    CuckooFilter *cf = obj;
    const char *chunk;
    size_t nchunk;
    CFHeader header;
    fillCFHeader(&header, cf);

    long long pos = 1;
    RedisModule_EmitAOF(aof, "CF.LOADCHUNK", "slb", key, pos, (const char *)&header, sizeof header);
    while ((chunk = CF_GetEncodedChunk(cf, &pos, &nchunk, MAX_SCANDUMP_SIZE))) {
        RedisModule_EmitAOF(aof, "CF.LOADCHUNK", "slb", key, pos, chunk, nchunk);
    }
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
    if (RedisModule_Init(ctx, "bf", REBLOOM_MODULE_VERSION, REDISMODULE_APIVER_1) !=
        REDISMODULE_OK) {
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
        BAIL("Invalid number of arguments passed", NULL);
    }

    for (int ii = 0; ii < argc; ii += 2) {
        if (!rsStrcasecmp(argv[ii], "initial_size")) {
            long long v;
            if (RedisModule_StringToLongLong(argv[ii + 1], &v) == REDISMODULE_ERR) {
                BAIL("Invalid argument for 'INITIAL_SIZE'", NULL); 
            }
            if (v > 0) {
                BFDefaultInitCapacity = v;
            } else {
                BAIL("INITIAL_SIZE must be > 0", NULL);
            }
        } else if (!rsStrcasecmp(argv[ii], "error_rate")) {
            double d;
            if (RedisModule_StringToDouble(argv[ii + 1], &d) == REDISMODULE_ERR) {
                BAIL("Invalid argument for 'ERROR_RATE'", NULL);
            } else if (d <= 0) {
                BAIL("ERROR_RATE must be > 0", NULL);
            } else {
                BFDefaultErrorRate = d;
            }
        } else if (!rsStrcasecmp(argv[ii], "cf_max_expansions")) {
            long long l;
            if (RedisModule_StringToLongLong(argv[ii + 1], &l) == REDISMODULE_ERR || l <= 0) {
                BAIL("Invalid argument for 'CF_MAX_EXPANSIONS'", NULL);
            }
            CFMaxExpansions = l;
        } else {
            BAIL("Unrecognized option", NULL);
        } 
    } 

#define CREATE_CMD(name, tgt, attr)                                                                \
    do {                                                                                           \
        if (RedisModule_CreateCommand(ctx, name, tgt, attr, 1, 1, 1) != REDISMODULE_OK) {          \
            return REDISMODULE_ERR;                                                                \
        }                                                                                          \
    } while (0)
#define CREATE_WRCMD(name, tgt) CREATE_CMD(name, tgt, "write deny-oom")
#define CREATE_ROCMD(name, tgt) CREATE_CMD(name, tgt, "readonly fast")

    CREATE_WRCMD("bf.reserve", BFReserve_RedisCommand);
    CREATE_WRCMD("bf.add", BFAdd_RedisCommand);
    CREATE_WRCMD("bf.madd", BFAdd_RedisCommand);
    CREATE_WRCMD("bf.insert", BFInsert_RedisCommand);
    CREATE_ROCMD("bf.exists", BFCheck_RedisCommand);
    CREATE_ROCMD("bf.mexists", BFCheck_RedisCommand);
    CREATE_ROCMD("bf.info", BFInfo_RedisCommand);

    // Bloom - Debug
    CREATE_ROCMD("bf.debug", BFDebug_RedisCommand);
    
    // Bloom - AOF
    CREATE_ROCMD("bf.scandump", BFScanDump_RedisCommand);
    CREATE_WRCMD("bf.loadchunk", BFLoadChunk_RedisCommand);

    // Cuckoo Filter commands
    CREATE_WRCMD("cf.reserve", CFReserve_RedisCommand);
    CREATE_WRCMD("cf.add", CFAdd_RedisCommand);
    CREATE_WRCMD("cf.addnx", CFAdd_RedisCommand);
    CREATE_WRCMD("cf.insert", CFInsert_RedisCommand);
    CREATE_WRCMD("cf.insertnx", CFInsert_RedisCommand);
    CREATE_ROCMD("cf.exists", CFCheck_RedisCommand);
    CREATE_ROCMD("cf.mexists", CFCheck_RedisCommand);
    CREATE_ROCMD("cf.count", CFCheck_RedisCommand);

    // Technically a write command, but doesn't change memory profile
    CREATE_CMD("cf.del", CFDel_RedisCommand, "write fast");

    CREATE_ROCMD("cf.compact", CFCompact_RedisCommand);
    // AOF:
    CREATE_ROCMD("cf.scandump", CFScanDump_RedisCommand);
    CREATE_WRCMD("cf.loadchunk", CFLoadChunk_RedisCommand);

    CREATE_ROCMD("cf.info", CFInfo_RedisCommand);
    CREATE_ROCMD("cf.debug", CFDebug_RedisCommand);
    
    CMSModule_onLoad(ctx, argv, argc);
    TopKModule_onLoad(ctx, argv, argc);

    static RedisModuleTypeMethods typeprocs = {.version = REDISMODULE_TYPE_METHOD_VERSION,
                                               .rdb_load = BFRdbLoad,
                                               .rdb_save = BFRdbSave,
                                               .aof_rewrite = BFAofRewrite,
                                               .free = BFFree,
                                               .mem_usage = BFMemUsage};
    BFType = RedisModule_CreateDataType(ctx, "MBbloom--", BF_MIN_GROWTH_ENC, &typeprocs);
    if (BFType == NULL) {
        return REDISMODULE_ERR;
    }

    static RedisModuleTypeMethods cfTypeProcs = {.version = REDISMODULE_TYPE_METHOD_VERSION,
                                                 .rdb_load = CFRdbLoad,
                                                 .rdb_save = CFRdbSave,
                                                 .aof_rewrite = CFAofRewrite,
                                                 .free = CFFree,
                                                 .mem_usage = CFMemUsage};
    CFType = RedisModule_CreateDataType(ctx, "MBbloomCF", CF_MIN_EXPANSION_VERSION, &cfTypeProcs);
    if (CFType == NULL) {
        return REDISMODULE_ERR;
    }
    return REDISMODULE_OK;
}
