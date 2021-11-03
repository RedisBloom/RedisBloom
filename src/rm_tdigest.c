#include <math.h>    // ceil, log10f
#include <stdlib.h>  // malloc
#include <strings.h> // strncasecmp

#include "rm_tdigest.h"
#include "rmutil/util.h"
#include "version.h"
#define REDISMODULE_MAIN
#include "redismodule.h"

// defining TD_ALLOC_H is used to change the t-digest allocator at compile time
// The define should be placed before including "tdigest.h" for the first time
#define TD_ALLOC_H
#define __td_malloc RedisModule_Alloc
#define __td_calloc RedisModule_Calloc
#define __td_realloc RedisModule_Realloc
#define __td_free RedisModule_Free

#include "tdigest.h"

RedisModuleType *TDigestSketchType;

/**
 * Helper method to check if key is empty and it's type.
 * On error the key is closed.
 */
static int _TDigest_KeyCheck(RedisModuleCtx *ctx, RedisModuleKey *key) {
    if (RedisModule_KeyType(key) == REDISMODULE_KEYTYPE_EMPTY) {
        RedisModule_CloseKey(key);
        RedisModule_ReplyWithError(ctx, "ERR T-Digest: key does not exist");
        return REDISMODULE_ERR;
    } else if (RedisModule_ModuleTypeGetType(key) != TDigestSketchType) {
        RedisModule_CloseKey(key);
        RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
        return REDISMODULE_ERR;
    }
    return REDISMODULE_OK;
}

/**
 * Command: TDIGEST.CREATE {key} {compression}
 *
 * Allocate the memory and initialize the t-digest.
 *
 * @param ctx Context in which Redis modules operate
 * @param argv Redis command arguments, as an array of strings
 * @param argc Redis command number of arguments
 * @return REDISMODULE_OK on success, or REDISMODULE_ERR  if the command failed
 */
int TDigestSketch_Create(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc != 3) {
        return RedisModule_WrongArity(ctx);
    }

    long long compression = 0;
    RedisModuleString *keyName = argv[1];
    RedisModuleKey *key = RedisModule_OpenKey(ctx, keyName, REDISMODULE_READ | REDISMODULE_WRITE);
    if (RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_EMPTY &&
        RedisModule_ModuleTypeGetType(key) != TDigestSketchType) {
        RedisModule_CloseKey(key);
        RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
        return REDISMODULE_ERR;
    }

    if (RedisModule_StringToLongLong(argv[2], &compression) != REDISMODULE_OK) {
        RedisModule_CloseKey(key);
        return RedisModule_ReplyWithError(ctx, "ERR T-Digest: error parsing compression parameter");
    }
    if (compression <= 0) {
        RedisModule_CloseKey(key);
        return RedisModule_ReplyWithError(
            ctx, "ERR T-Digest: compression parameter needs to be a positive integer");
    }
    td_histogram_t *tdigest = td_new(compression);
    ;
    if (RedisModule_ModuleTypeSetValue(key, TDigestSketchType, tdigest) != REDISMODULE_OK) {
        RedisModule_CloseKey(key);
        return RedisModule_ReplyWithError(ctx, "ERR T-Digest: error setting value");
    }
    RedisModule_CloseKey(key);
    RedisModule_ReplicateVerbatim(ctx);
    RedisModule_ReplyWithSimpleString(ctx, "OK");
    return REDISMODULE_OK;
}

/**
 * Command: TDIGEST.RESET {key}
 *
 * Reset a histogram to zero - empty out a histogram and re-initialize it.
 *
 * @param ctx Context in which Redis modules operate
 * @param argv Redis command arguments, as an array of strings
 * @param argc Redis command number of arguments
 * @return REDISMODULE_OK on success, or REDISMODULE_ERR  if the command failed
 */
int TDigestSketch_Reset(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc != 2) {
        return RedisModule_WrongArity(ctx);
    }
    RedisModuleString *keyName = argv[1];
    RedisModuleKey *key = RedisModule_OpenKey(ctx, keyName, REDISMODULE_READ | REDISMODULE_WRITE);

    if (_TDigest_KeyCheck(ctx, key) != REDISMODULE_OK)
        return REDISMODULE_ERR;

    td_histogram_t *tdigest = RedisModule_ModuleTypeGetValue(key);
    td_reset(tdigest);
    RedisModule_CloseKey(key);
    RedisModule_ReplicateVerbatim(ctx);
    RedisModule_ReplyWithSimpleString(ctx, "OK");
    return REDISMODULE_OK;
}

/**
 * Command: TDIGEST.ADD {key} {val} {weight} [ {val} {weight} ] ...
 *
 * Adds one or more samples to a histogram.
 *
 * @param ctx Context in which Redis modules operate
 * @param argv Redis command arguments, as an array of strings
 * @param argc Redis command number of arguments
 * @return REDISMODULE_OK on success, or REDISMODULE_ERR  if the command failed
 */
int TDigestSketch_Add(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    // Number of arguments needs to be even
    if (argc < 4 || (argc % 2 != 0)) {
        return RedisModule_WrongArity(ctx);
    }
    RedisModuleString *keyName = argv[1];
    RedisModuleKey *key = RedisModule_OpenKey(ctx, keyName, REDISMODULE_READ | REDISMODULE_WRITE);

    if (_TDigest_KeyCheck(ctx, key) != REDISMODULE_OK)
        return REDISMODULE_ERR;

    td_histogram_t *tdigest = RedisModule_ModuleTypeGetValue(key);
    double val = 0.0, weight = 0.0;
    for (int i = 2; i < argc; i += 2) {
        if (RedisModule_StringToDouble(argv[i], &val) != REDISMODULE_OK) {
            RedisModule_CloseKey(key);
            return RedisModule_ReplyWithError(ctx, "ERR T-Digest: error parsing val parameter");
        }
        if (RedisModule_StringToDouble(argv[i + 1], &weight) != REDISMODULE_OK) {
            RedisModule_CloseKey(key);
            return RedisModule_ReplyWithError(ctx, "ERR T-Digest: error parsing weight parameter");
        }
        td_add(tdigest, val, weight);
    }
    RedisModule_CloseKey(key);
    RedisModule_ReplicateVerbatim(ctx);
    RedisModule_ReplyWithSimpleString(ctx, "OK");
    return REDISMODULE_OK;
}

/**
 * Command: TDIGEST.MERGE {to-key} {from-key}
 *
 * Merges all of the values from 'from' to 'this' histogram.
 *
 * @param ctx Context in which Redis modules operate
 * @param argv Redis command arguments, as an array of strings
 * @param argc Redis command number of arguments
 * @return REDISMODULE_OK on success, or REDISMODULE_ERR  if the command failed
 */
int TDigestSketch_Merge(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc != 3) {
        return RedisModule_WrongArity(ctx);
    }
    RedisModuleString *keyNameTo = argv[1];
    RedisModuleKey *keyTo =
        RedisModule_OpenKey(ctx, keyNameTo, REDISMODULE_READ | REDISMODULE_WRITE);

    if (_TDigest_KeyCheck(ctx, keyTo))
        return REDISMODULE_ERR;

    td_histogram_t *tdigestTo = RedisModule_ModuleTypeGetValue(keyTo);

    RedisModuleString *keyNameFrom = argv[2];
    RedisModuleKey *keyFrom = RedisModule_OpenKey(ctx, keyNameFrom, REDISMODULE_READ);
    if (_TDigest_KeyCheck(ctx, keyFrom)) {
        RedisModule_CloseKey(keyTo);
        return REDISMODULE_ERR;
    }
    td_histogram_t *tdigestFrom = RedisModule_ModuleTypeGetValue(keyFrom);
    td_merge(tdigestTo, tdigestFrom);
    RedisModule_CloseKey(keyTo);
    RedisModule_CloseKey(keyFrom);
    RedisModule_ReplicateVerbatim(ctx);
    RedisModule_ReplyWithSimpleString(ctx, "OK");
    return REDISMODULE_OK;
}

/**
 * Command: TDIGEST.MIN {key}
 *
 * Get minimum value from the histogram.  Will return __DBL_MAX__ if the histogram is empty.
 *
 * @param ctx Context in which Redis modules operate
 * @param argv Redis command arguments, as an array of strings
 * @param argc Redis command number of arguments
 * @return REDISMODULE_OK on success, or REDISMODULE_ERR  if the command failed
 */
int TDigestSketch_Min(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc != 2) {
        return RedisModule_WrongArity(ctx);
    }
    RedisModuleString *keyName = argv[1];
    RedisModuleKey *key = RedisModule_OpenKey(ctx, keyName, REDISMODULE_READ);

    if (_TDigest_KeyCheck(ctx, key) != REDISMODULE_OK)
        return REDISMODULE_ERR;

    td_histogram_t *tdigest = RedisModule_ModuleTypeGetValue(key);
    const double min = td_min(tdigest);
    RedisModule_CloseKey(key);
    RedisModule_ReplyWithDouble(ctx, min);
    return REDISMODULE_OK;
}

/**
 * Command: TDIGEST.MAX {key}
 *
 * Get maximum value from the histogram.  Will return __DBL_MIN__ if the histogram is empty.
 *
 * @param ctx Context in which Redis modules operate
 * @param argv Redis command arguments, as an array of strings
 * @param argc Redis command number of arguments
 * @return REDISMODULE_OK on success, or REDISMODULE_ERR  if the command failed
 */
int TDigestSketch_Max(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc != 2) {
        return RedisModule_WrongArity(ctx);
    }
    RedisModuleString *keyName = argv[1];
    RedisModuleKey *key = RedisModule_OpenKey(ctx, keyName, REDISMODULE_READ);

    if (_TDigest_KeyCheck(ctx, key) != REDISMODULE_OK)
        return REDISMODULE_ERR;

    td_histogram_t *tdigest = RedisModule_ModuleTypeGetValue(key);
    const double max = td_max(tdigest);
    RedisModule_CloseKey(key);
    RedisModule_ReplyWithDouble(ctx, max);
    return REDISMODULE_OK;
}

/**
 * Command: TDIGEST.QUANTILE {key} {quantile}
 *
 * Returns an estimate of the cutoff such that a specified fraction of the data
 * added to this TDigest would be less than or equal to the cutoff.
 *
 * @param ctx Context in which Redis modules operate
 * @param argv Redis command arguments, as an array of strings
 * @param argc Redis command number of arguments
 * @return REDISMODULE_OK on success, or REDISMODULE_ERR  if the command failed
 */
int TDigestSketch_Quantile(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc != 3) {
        return RedisModule_WrongArity(ctx);
    }
    RedisModuleString *keyName = argv[1];
    RedisModuleKey *key = RedisModule_OpenKey(ctx, keyName, REDISMODULE_READ);

    if (_TDigest_KeyCheck(ctx, key) != REDISMODULE_OK)
        return REDISMODULE_ERR;

    td_histogram_t *tdigest = RedisModule_ModuleTypeGetValue(key);

    double quantile = 0.0;
    if (RedisModule_StringToDouble(argv[2], &quantile) != REDISMODULE_OK) {
        RedisModule_CloseKey(key);
        return RedisModule_ReplyWithError(ctx, "ERR T-Digest: error parsing quantile");
    }
    const double value = td_quantile(tdigest, quantile);
    RedisModule_CloseKey(key);
    RedisModule_ReplyWithDouble(ctx, value);
    return REDISMODULE_OK;
}

/**
 * Command: TDIGEST.CDF {key} {value}
 *
 * Returns the fraction of all points added which are <= value.
 *
 * @param ctx Context in which Redis modules operate
 * @param argv Redis command arguments, as an array of strings
 * @param argc Redis command number of arguments
 * @return REDISMODULE_OK on success, or REDISMODULE_ERR  if the command failed
 */
int TDigestSketch_Cdf(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc != 3) {
        return RedisModule_WrongArity(ctx);
    }
    RedisModuleString *keyName = argv[1];
    RedisModuleKey *key = RedisModule_OpenKey(ctx, keyName, REDISMODULE_READ);

    if (_TDigest_KeyCheck(ctx, key) != REDISMODULE_OK)
        return REDISMODULE_ERR;

    td_histogram_t *tdigest = RedisModule_ModuleTypeGetValue(key);

    double cdf = 0.0;
    if (RedisModule_StringToDouble(argv[2], &cdf) != REDISMODULE_OK) {
        RedisModule_CloseKey(key);
        return RedisModule_ReplyWithError(ctx, "ERR T-Digest: error parsing cdf value");
    }

    const double value = td_cdf(tdigest, cdf);
    RedisModule_CloseKey(key);
    RedisModule_ReplyWithDouble(ctx, value);
    return REDISMODULE_OK;
}

/**
 * Command: TDIGEST.INFO {key}
 *
 * Returns compression, capacity, total merged and unmerged nodes, the total compressions
 * made up to date on that key, and merged and unmerged weight.
 *
 * @param ctx Context in which Redis modules operate
 * @param argv Redis command arguments, as an array of strings
 * @param argc Redis command number of arguments
 * @return REDISMODULE_OK on success, or REDISMODULE_ERR  if the command failed
 */
int TDigestSketch_Info(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc != 2) {
        return RedisModule_WrongArity(ctx);
    }
    RedisModuleString *keyName = argv[1];
    RedisModuleKey *key = RedisModule_OpenKey(ctx, keyName, REDISMODULE_READ);

    if (_TDigest_KeyCheck(ctx, key) != REDISMODULE_OK)
        return REDISMODULE_ERR;

    td_histogram_t *tdigest = RedisModule_ModuleTypeGetValue(key);

    RedisModule_ReplyWithArray(ctx, 7 * 2);
    RedisModule_ReplyWithSimpleString(ctx, "Compression");
    RedisModule_ReplyWithLongLong(ctx, tdigest->compression);
    RedisModule_ReplyWithSimpleString(ctx, "Capacity");
    RedisModule_ReplyWithLongLong(ctx, tdigest->cap);
    RedisModule_ReplyWithSimpleString(ctx, "Merged nodes");
    RedisModule_ReplyWithLongLong(ctx, tdigest->merged_nodes);
    RedisModule_ReplyWithSimpleString(ctx, "Unmerged nodes");
    RedisModule_ReplyWithLongLong(ctx, tdigest->unmerged_nodes);
    RedisModule_ReplyWithSimpleString(ctx, "Merged weight");
    RedisModule_ReplyWithDouble(ctx, tdigest->merged_weight);
    RedisModule_ReplyWithSimpleString(ctx, "Unmerged weight");
    RedisModule_ReplyWithDouble(ctx, tdigest->unmerged_weight);
    RedisModule_ReplyWithSimpleString(ctx, "Total compressions");
    RedisModule_ReplyWithLongLong(ctx, tdigest->total_compressions);
    RedisModule_CloseKey(key);
    return REDISMODULE_OK;
}

void TDigestRdbSave(RedisModuleIO *rdb, void *value) {
    td_histogram_t *tdigest = value;
    // ensure there is no unmerged node
    td_compress(tdigest);
    // compression is a setting used to configure the size of centroids when merged.
    RedisModule_SaveDouble(rdb, tdigest->compression);
    RedisModule_SaveDouble(rdb, tdigest->min);
    RedisModule_SaveDouble(rdb, tdigest->max);

    // cap is the total size of nodes
    RedisModule_SaveSigned(rdb, tdigest->cap);
    // merged_nodes is the number of merged nodes at the front of nodes.
    RedisModule_SaveSigned(rdb, tdigest->merged_nodes);
    // unmerged_nodes is the number of buffered nodes.
    RedisModule_SaveSigned(rdb, tdigest->unmerged_nodes);

    // we run the merge in reverse every other merge to avoid left-to-right bias in merging
    RedisModule_SaveSigned(rdb, tdigest->total_compressions);

    RedisModule_SaveDouble(rdb, tdigest->merged_weight);
    RedisModule_SaveDouble(rdb, tdigest->unmerged_weight);

    for (size_t i = 0; i < tdigest->merged_nodes; i++) {
        const node_t n = tdigest->nodes[i];
        RedisModule_SaveDouble(rdb, n.mean);
    }
    for (size_t i = 0; i < tdigest->merged_nodes; i++) {
        const node_t n = tdigest->nodes[i];
        RedisModule_SaveDouble(rdb, n.count);
    }
}

void *TDigestRdbLoad(RedisModuleIO *rdb, int encver) {
    /* Load the network layout. */
    const double compression = RedisModule_LoadDouble(rdb);
    td_histogram_t *tdigest = td_new(compression);
    tdigest->min = RedisModule_LoadDouble(rdb);
    tdigest->min = RedisModule_LoadDouble(rdb);

    // cap is the total size of nodes
    tdigest->cap = RedisModule_LoadSigned(rdb);

    // merged_nodes is the number of merged nodes at the front of nodes.
    tdigest->merged_nodes = RedisModule_LoadSigned(rdb);

    // unmerged_nodes is the number of buffered nodes.
    tdigest->unmerged_nodes = RedisModule_LoadSigned(rdb);

    // we run the merge in reverse every other merge to avoid left-to-right bias in merging
    tdigest->total_compressions = RedisModule_LoadSigned(rdb);

    tdigest->merged_weight = RedisModule_LoadDouble(rdb);
    tdigest->unmerged_weight = RedisModule_LoadDouble(rdb);

    for (size_t i = 0; i < tdigest->merged_nodes; i++) {
        tdigest->nodes[i].mean = RedisModule_LoadDouble(rdb);
    }
    for (size_t i = 0; i < tdigest->merged_nodes; i++) {
        tdigest->nodes[i].count = RedisModule_LoadDouble(rdb);
    }
    return tdigest;
}

void TDigestFree(void *value) {
    td_histogram_t *tdigest = (td_histogram_t *)value;
    td_free(tdigest);
}

size_t TDigestMemUsage(const void *value) {
    td_histogram_t *tdigest = (td_histogram_t *)value;
    size_t size = sizeof(tdigest);
    size += (2 * (tdigest->cap * sizeof(double)));
    return size;
}

int TDigestModule_onLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    // TODO: add option to set defaults from command line and in program
    RedisModuleTypeMethods tm = {.version = REDISMODULE_TYPE_METHOD_VERSION,
                                 .rdb_load = TDigestRdbLoad,
                                 .rdb_save = TDigestRdbSave,
                                 .aof_rewrite = RMUtil_DefaultAofRewrite,
                                 .mem_usage = TDigestMemUsage,
                                 .free = TDigestFree};

    TDigestSketchType = RedisModule_CreateDataType(ctx, "TDIS-TYPE", TDIGEST_ENC_VER, &tm);
    if (TDigestSketchType == NULL)
        return REDISMODULE_ERR;

    RMUtil_RegisterWriteDenyOOMCmd(ctx, "tdigest.create", TDigestSketch_Create);
    RMUtil_RegisterWriteDenyOOMCmd(ctx, "tdigest.add", TDigestSketch_Add);
    RMUtil_RegisterWriteDenyOOMCmd(ctx, "tdigest.reset", TDigestSketch_Reset);
    RMUtil_RegisterWriteDenyOOMCmd(ctx, "tdigest.merge", TDigestSketch_Merge);
    RMUtil_RegisterReadCmd(ctx, "tdigest.min", TDigestSketch_Min);
    RMUtil_RegisterReadCmd(ctx, "tdigest.max", TDigestSketch_Max);
    RMUtil_RegisterReadCmd(ctx, "tdigest.quantile", TDigestSketch_Quantile);
    RMUtil_RegisterReadCmd(ctx, "tdigest.cdf", TDigestSketch_Cdf);
    RMUtil_RegisterReadCmd(ctx, "tdigest.info", TDigestSketch_Info);
    return REDISMODULE_OK;
}
