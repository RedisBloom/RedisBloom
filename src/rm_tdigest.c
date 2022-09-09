
#include "rm_tdigest.h"
#include "rmutil/util.h"
#include "version.h"

#include "redismodule.h"

#include <math.h>
#include <stdlib.h>
#include <strings.h>
#include <stdbool.h>

// defining TD_ALLOC_H is used to change the t-digest allocator at compile time
// The define should be placed before including "tdigest.h" for the first time
#define TD_ALLOC_H
#define __td_malloc RedisModule_Alloc
#define __td_calloc RedisModule_Calloc
#define __td_realloc RedisModule_Realloc
#define __td_free RedisModule_Free
#define TD_DEFAULT_COMPRESSION 100

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
 * Helper method to parse compression parameter.
 */
static int _TDigest_ParseCompressionParameter(RedisModuleCtx *ctx, const RedisModuleString *param,
                                              long long *compression) {
    if (RedisModule_StringToLongLong(param, compression) != REDISMODULE_OK) {
        RedisModule_ReplyWithError(ctx, "ERR T-Digest: error parsing compression parameter");
        return REDISMODULE_ERR;
    }
    if (*compression <= 0) {
        RedisModule_ReplyWithError(
            ctx, "ERR T-Digest: compression parameter needs to be a positive integer");
        return REDISMODULE_ERR;
    }
    return REDISMODULE_OK;
}

/**
 * Command: TDIGEST.CREATE {key} [{compression}]
 *
 * Allocate the memory and initialize the t-digest.
 *
 * @param ctx Context in which Redis modules operate
 * @param argv Redis command arguments, as an array of strings
 * @param argc Redis command number of arguments
 * @return REDISMODULE_OK on success, or REDISMODULE_ERR  if the command failed
 */
int TDigestSketch_Create(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc != 2 && argc != 4) {
        return RedisModule_WrongArity(ctx);
    }

    RedisModuleString *keyName = argv[1];
    td_histogram_t *tdigest = NULL;
    RedisModuleKey *key = RedisModule_OpenKey(ctx, keyName, REDISMODULE_READ | REDISMODULE_WRITE);
    if (RedisModule_KeyType(key) != REDISMODULE_KEYTYPE_EMPTY) {
        if (RedisModule_ModuleTypeGetType(key) == TDigestSketchType) {
            RedisModule_ReplyWithError(ctx, "ERR T-Digest: key already exists");
        } else {
            RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
        }
        RedisModule_CloseKey(key);
        return REDISMODULE_ERR;
    }
    long long compression = TD_DEFAULT_COMPRESSION;
    // if argc is 3 we have the compression parameter defined
    // otherwise we use the default value
    if (argc == 4) {
        int compression_loc = RMUtil_ArgIndex("COMPRESSION", argv + 2, argc - 2);
        if (compression_loc == -1) {
            RedisModule_ReplyWithError(ctx, "ERR T-Digest: wrong keyword");
            RedisModule_CloseKey(key);
            return REDISMODULE_ERR;
        }
        if (_TDigest_ParseCompressionParameter(ctx, argv[3], &compression) != REDISMODULE_OK) {
            RedisModule_CloseKey(key);
            return REDISMODULE_ERR;
        }
    }
    tdigest = td_new(compression);
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
 * Command: TDIGEST.MERGE {destination} numkeys from-key [from-key ...] [COMPRESSION
 * compression] [OVERRIDE]
 *
 * Merges all of the values from 'from' keys to 'destination-key' sketch.
 * It is mandatory to provide the number of input keys (numkeys) before
 * passing the input keys and the other (optional) arguments.
 * If destination already exists its values are merged with the input keys. If you wish to override
 * the destination key contents use the `OVERRIDE` parameter.
 *
 * @param ctx Context in which Redis modules operate
 * @param argv Redis command arguments, as an array of strings
 * @param argc Redis command number of arguments
 * @return REDISMODULE_OK on success, or REDISMODULE_ERR  if the command failed
 */
int TDigestSketch_Merge(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc < 4) {
        return RedisModule_WrongArity(ctx);
    }
    RedisModuleString *keyNameDestination = argv[1];
    td_histogram_t *tdigestTo = NULL;
    td_histogram_t *tdigestToStart = NULL;
    int current_pos = 0;
    int res = REDISMODULE_ERR;
    td_histogram_t **from_tdigests = NULL;
    RedisModuleKey **from_keys = NULL;
    bool to_exists = false;
    RedisModuleKey *keyDestination =
        RedisModule_OpenKey(ctx, keyNameDestination, REDISMODULE_READ | REDISMODULE_WRITE);
    // check if key existed already. If so, confirm it's of the proper type
    if (RedisModule_KeyType(keyDestination) != REDISMODULE_KEYTYPE_EMPTY) {
        if (RedisModule_ModuleTypeGetType(keyDestination) != TDigestSketchType) {
            RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
            goto cleanup;
        }
        tdigestToStart = RedisModule_ModuleTypeGetValue(keyDestination);
        to_exists = true;
    }
    // parse numkeys
    long long numkeys = 1;
    if (RedisModule_StringToLongLong(argv[2], &numkeys) != REDISMODULE_OK) {
        RedisModule_ReplyWithError(ctx, "ERR T-Digest: error parsing numkeys");
        goto cleanup;
    }
    if (numkeys <= 0) {
        RedisModule_ReplyWithError(ctx, "ERR T-Digest: numkeys needs to be a positive integer");
        goto cleanup;
    }
    if (numkeys > (argc - 3)) {
        RedisModule_WrongArity(ctx);
        goto cleanup;
    }
    long long compression = 0;
    if (to_exists) {
        compression = tdigestToStart->compression;
    }
    // If no compression value is passed and the origin key does not exist,
    // the used compression will the maximal value amongst all inputs.
    bool use_max_compression = to_exists ? false : true;
    bool override = false;
    const int start_remaining_args = numkeys + 3;
    if (start_remaining_args < argc) {
        int compression_loc = RMUtil_ArgIndex("COMPRESSION", argv + start_remaining_args,
                                              argc - start_remaining_args);
        if (compression_loc > -1) {
            use_max_compression = false;
            if (start_remaining_args + compression_loc + 1 >= argc) {
                RedisModule_WrongArity(ctx);
                goto cleanup;
            }
            if (_TDigest_ParseCompressionParameter(ctx,
                                                   argv[start_remaining_args + compression_loc + 1],
                                                   &compression) == REDISMODULE_ERR) {
                goto cleanup;
            }
        }
        int override_loc =
            RMUtil_ArgIndex("OVERRIDE", argv + start_remaining_args, argc - start_remaining_args);
        if (override_loc > -1) {
            override = true;
            // in the case of OVERRIDE being present but compression not implicity specified we
            // default to the rule of max of inputs
            if (compression_loc == -1) {
                use_max_compression = true;
                compression = 0;
            }
        }
        if (compression_loc == -1 && override_loc == -1) {
            RedisModule_ReplyWithError(ctx, "ERR T-Digest: wrong keyword");
            goto cleanup;
        }
    }
    from_tdigests = (td_histogram_t **)__td_calloc(numkeys, sizeof(td_histogram_t *));
    from_keys = (RedisModuleKey **)__td_calloc(numkeys, sizeof(RedisModuleKey *));
    for (current_pos = 0; current_pos < numkeys; current_pos++) {
        RedisModuleString *keyNameFrom = argv[current_pos + 3];
        from_keys[current_pos] = RedisModule_OpenKey(ctx, keyNameFrom, REDISMODULE_READ);
        if (_TDigest_KeyCheck(ctx, from_keys[current_pos])) {
            goto cleanup;
        }
        from_tdigests[current_pos] = RedisModule_ModuleTypeGetValue(from_keys[current_pos]);
        // if we haven't specified the COMPRESSION parameter we will use the
        // highest possible compression
        if (use_max_compression) {
            const long long td_compression = from_tdigests[current_pos]->compression;
            compression = td_compression > compression ? td_compression : compression;
        }
    }
    tdigestTo = td_new(compression);
    if (tdigestToStart != NULL && override == false) {
        td_merge(tdigestTo, tdigestToStart);
    }

    for (long long i = 0; i < numkeys; i++) {
        td_merge(tdigestTo, from_tdigests[i]);
    }
    if (RedisModule_ModuleTypeSetValue(keyDestination, TDigestSketchType, tdigestTo) !=
        REDISMODULE_OK) {
        RedisModule_ReplyWithError(ctx, "ERR T-Digest: error setting value");
        goto cleanup;
    }
    res = REDISMODULE_OK;
    RedisModule_ReplicateVerbatim(ctx);
    RedisModule_ReplyWithSimpleString(ctx, "OK");
cleanup:
    RedisModule_CloseKey(keyDestination);
    for (size_t i = 0; i < current_pos; i++) {
        RedisModuleString *keyNameFrom = argv[i + 3];
        // If the key is not the same as the destination key, close it
        if (RedisModule_StringCompare(keyNameDestination, keyNameFrom) != 0) {
            RedisModule_CloseKey(from_keys[i]);
        }
    }
    if (from_tdigests)
        __td_free(from_tdigests);
    if (from_keys)
        __td_free(from_keys);

    return res;
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
 * Get maximum value from the histogram.  Will return -__DBL_MAX__ if the histogram is empty.
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

#if 0 // unused
static int double_cmpfunc(const void *a, const void *b) {
    if (*(double *)a > *(double *)b)
        return 1;
    else if (*(double *)a < *(double *)b)
        return -1;
    else
        return 0;
}
#endif

/**
 * Command: TDIGEST.QUANTILE {key} {quantile} [{quantile2}...]
 *
 * Returns an estimate of the cutoff such that a specified fraction of the data
 * added to this TDigest would be less than or equal to the cutoff quantiles.
 * The command returns an array of results: each element of the returned array
 * populated with quantile_1, cutoff_1, quantile_2, cutoff_2, ..., quantile_N, cutoff_N.
 *
 * @param ctx Context in which Redis modules operate
 * @param argv Redis command arguments, as an array of strings
 * @param argc Redis command number of arguments
 * @return REDISMODULE_OK on success, or REDISMODULE_ERR  if the command failed
 */
int TDigestSketch_Quantile(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc < 3) {
        return RedisModule_WrongArity(ctx);
    }
    RedisModuleString *keyName = argv[1];
    RedisModuleKey *key = RedisModule_OpenKey(ctx, keyName, REDISMODULE_READ);

    if (_TDigest_KeyCheck(ctx, key) != REDISMODULE_OK)
        return REDISMODULE_ERR;

    td_histogram_t *tdigest = RedisModule_ModuleTypeGetValue(key);

    const size_t n_quantiles = argc - 2;
    double *quantiles = (double *)__td_calloc(n_quantiles, sizeof(double));

    for (int i = 0; i < n_quantiles; ++i) {
        if (RedisModule_StringToDouble(argv[2 + i], &quantiles[i]) != REDISMODULE_OK) {
            RedisModule_CloseKey(key);
            __td_free(quantiles);
            return RedisModule_ReplyWithError(ctx, "ERR T-Digest: error parsing quantile");
        }
    }
    double *values = (double *)__td_calloc(n_quantiles, sizeof(double));
    for (int i = 0; i < n_quantiles; ++i) {
        int start = i;
        while (i < n_quantiles - 1 && quantiles[i] <= quantiles[i + 1]) {
            ++i;
        }
        td_quantiles(tdigest, quantiles + start, values + start, i - start + 1);
    }
    RedisModule_CloseKey(key);
    RedisModule_ReplyWithArray(ctx, n_quantiles);
    for (int i = 0; i < n_quantiles; ++i) {
        RedisModule_ReplyWithDouble(ctx, values[i]);
    }
    __td_free(values);
    __td_free(quantiles);
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
 * Command: TDIGEST.TRIMMED_MEAN {key} {low_cut_percentile} {high_cut_percentile}
 *
 * Returns the trimmed mean ignoring values outside given cutoff upper and lower limits.
 *
 * @param ctx Context in which Redis modules operate
 * @param argv Redis command arguments, as an array of strings
 * @param argc Redis command number of arguments
 * @return REDISMODULE_OK on success, or REDISMODULE_ERR  if the command failed
 */
int TDigestSketch_TrimmedMean(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc != 4) {
        return RedisModule_WrongArity(ctx);
    }
    RedisModuleString *keyName = argv[1];
    RedisModuleKey *key = RedisModule_OpenKey(ctx, keyName, REDISMODULE_READ);

    if (_TDigest_KeyCheck(ctx, key) != REDISMODULE_OK)
        return REDISMODULE_ERR;

    td_histogram_t *tdigest = RedisModule_ModuleTypeGetValue(key);

    double low_cut_percentile = 0.0;
    double high_cut_percentile = 0.0;
    if (RedisModule_StringToDouble(argv[2], &low_cut_percentile) != REDISMODULE_OK) {
        RedisModule_CloseKey(key);
        return RedisModule_ReplyWithError(ctx, "ERR T-Digest: error parsing low_cut_percentile");
    }
    if (RedisModule_StringToDouble(argv[3], &high_cut_percentile) != REDISMODULE_OK) {
        RedisModule_CloseKey(key);
        return RedisModule_ReplyWithError(ctx, "ERR T-Digest: error parsing high_cut_percentile");
    }
    if (low_cut_percentile < 0.0 || low_cut_percentile > 1.0 || high_cut_percentile < 0.0 ||
        high_cut_percentile > 1.0) {
        RedisModule_CloseKey(key);
        return RedisModule_ReplyWithError(
            ctx, "ERR T-Digest: low_cut_percentile and high_cut_percentile should be in [0,1]");
    }
    if (low_cut_percentile > high_cut_percentile) {
        RedisModule_CloseKey(key);
        return RedisModule_ReplyWithError(
            ctx, "ERR T-Digest: low_cut_percentile should be lower than high_cut_percentile");
    }
    const double value = td_trimmed_mean(tdigest, low_cut_percentile, high_cut_percentile);
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
        const double mean = tdigest->nodes_mean[i];
        RedisModule_SaveDouble(rdb, mean);
    }
    for (size_t i = 0; i < tdigest->merged_nodes; i++) {
        const double count = tdigest->nodes_weight[i];
        RedisModule_SaveDouble(rdb, count);
    }
}

void *TDigestRdbLoad(RedisModuleIO *rdb, int encver) {
    /* Load the network layout. */
    const double compression = RedisModule_LoadDouble(rdb);
    td_histogram_t *tdigest = td_new(compression);
    tdigest->min = RedisModule_LoadDouble(rdb);
    tdigest->max = RedisModule_LoadDouble(rdb);

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
        tdigest->nodes_mean[i] = RedisModule_LoadDouble(rdb);
    }
    for (size_t i = 0; i < tdigest->merged_nodes; i++) {
        tdigest->nodes_weight[i] = RedisModule_LoadDouble(rdb);
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
    RMUtil_RegisterReadCmd(ctx, "tdigest.trimmed_mean", TDigestSketch_TrimmedMean);
    RMUtil_RegisterReadCmd(ctx, "tdigest.info", TDigestSketch_Info);
    return REDISMODULE_OK;
}
