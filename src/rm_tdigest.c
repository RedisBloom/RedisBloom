/*
 * Copyright (c) 2006-Present, Redis Ltd.
 * All rights reserved.
 *
 * Licensed under your choice of (a) the Redis Source Available License 2.0
 * (RSALv2); or (b) the Server Side Public License v1 (SSPLv1); or (c) the
 * GNU Affero General Public License v3 (AGPLv3).
 */

#include "rm_tdigest.h"
#include "rmutil/util.h"
#include "version.h"
#include "rm_cms.h"
#include "cmd_info/command_info.h"

#include "redismodule.h"
#include "common.h"

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
size_t TDigestMemUsage(const void *value);

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

double _halfRoundDown(double f) {
    double int_part;
    double frac_part = modf(f, &int_part);

    if (fabs(frac_part) <= 0.5)
        return int_part;

    return int_part >= 0.0 ? int_part + 1.0 : int_part - 1.0;
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
    if (td_init(compression, &tdigest) != 0) {
        RedisModule_CloseKey(key);
        return RedisModule_ReplyWithError(ctx, "ERR T-Digest: allocation failed");
    }
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
 * Command: TDIGEST.ADD {key} {val} [ {val} ] ...
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
    if (argc < 3) {
        return RedisModule_WrongArity(ctx);
    }
    RedisModuleString *keyName = argv[1];
    RedisModuleKey *key = RedisModule_OpenKey(ctx, keyName, REDISMODULE_READ | REDISMODULE_WRITE);

    if (_TDigest_KeyCheck(ctx, key) != REDISMODULE_OK)
        return REDISMODULE_ERR;
    const size_t n_values = argc - 2;
    double *vals = (double *)__td_calloc(n_values, sizeof(double));

    for (int i = 0; i < n_values; ++i) {
        if ((RedisModule_StringToDouble(argv[2 + i], &vals[i]) != REDISMODULE_OK) ||
            isnan(vals[i])) {
            RedisModule_CloseKey(key);
            __td_free(vals);
            return RedisModule_ReplyWithError(ctx, "ERR T-Digest: error parsing val parameter");
        }
        if (vals[i] < -__DBL_MAX__ || vals[i] > __DBL_MAX__) {
            RedisModule_CloseKey(key);
            __td_free(vals);
            return RedisModule_ReplyWithError(
                ctx, "ERR T-Digest: val parameter needs to be a finite number");
        }
    }

    td_histogram_t *tdigest = RedisModule_ModuleTypeGetValue(key);
    for (int i = 0; i < n_values; ++i) {
        if (td_add(tdigest, vals[i], 1) != 0) {
            RedisModule_CloseKey(key);
            __td_free(vals);
            return RedisModule_ReplyWithError(ctx, "ERR T-Digest: overflow detected");
        }
    }
    RedisModule_CloseKey(key);
    __td_free(vals);
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
    bool to_exists = false;
    RedisModuleKey *keyDestination = RedisModule_OpenKey(ctx, keyNameDestination, REDISMODULE_READ);
    // check if key existed already. If so, confirm it's of the proper type
    if (RedisModule_KeyType(keyDestination) != REDISMODULE_KEYTYPE_EMPTY) {
        if (RedisModule_ModuleTypeGetType(keyDestination) != TDigestSketchType) {
            RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
            RedisModule_CloseKey(keyDestination);
            goto cleanup;
        }
        tdigestToStart = RedisModule_ModuleTypeGetValue(keyDestination);
        to_exists = true;
    }
    RedisModule_CloseKey(keyDestination);
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
    long long compression = to_exists ? tdigestToStart->compression : 0;
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
    for (current_pos = 0; current_pos < numkeys; current_pos++) {
        RedisModuleString *keyNameFrom = argv[current_pos + 3];
        // If the key is not the same as the destination key, open it
        // otherwise the key was already open
        if (RedisModule_StringCompare(keyNameDestination, keyNameFrom) != 0) {
            RedisModuleKey *current_key = RedisModule_OpenKey(ctx, keyNameFrom, REDISMODULE_READ);
            if (_TDigest_KeyCheck(ctx, current_key)) {
                goto cleanup;
            }
            from_tdigests[current_pos] = RedisModule_ModuleTypeGetValue(current_key);
            RedisModule_CloseKey(current_key);
        } else {
            if (to_exists) {
                from_tdigests[current_pos] = tdigestToStart;
            } else {
                RedisModule_ReplyWithError(ctx, "ERR T-Digest: key does not exist");
                goto cleanup;
            }
        }
        // if we haven't specified the COMPRESSION parameter we will use the
        // highest possible compression
        if (use_max_compression) {
            const long long td_compression = from_tdigests[current_pos]->compression;
            compression = td_compression > compression ? td_compression : compression;
        }
    }
    if (td_init(compression, &tdigestTo) != 0) {
        RedisModule_ReplyWithError(ctx, "ERR T-Digest: allocation of destination digest failed");
        goto cleanup;
    }
    if (tdigestToStart != NULL && override == false) {
        td_merge(tdigestTo, tdigestToStart);
    }

    for (long long i = 0; i < numkeys; i++) {
        if (from_tdigests[i] != NULL) {
            if (td_merge(tdigestTo, from_tdigests[i]) != 0) {
                td_free(tdigestTo);
                RedisModule_ReplyWithError(ctx, "ERR T-Digest: overflow detected");
                goto cleanup;
            }
        }
    }
    keyDestination = RedisModule_OpenKey(ctx, keyNameDestination, REDISMODULE_WRITE);
    if (RedisModule_ModuleTypeSetValue(keyDestination, TDigestSketchType, tdigestTo) !=
        REDISMODULE_OK) {
        RedisModule_CloseKey(keyDestination);
        RedisModule_ReplyWithError(ctx, "ERR T-Digest: error setting value");
        goto cleanup;
    }
    RedisModule_CloseKey(keyDestination);
    res = REDISMODULE_OK;
    RedisModule_ReplicateVerbatim(ctx);
    RedisModule_ReplyWithSimpleString(ctx, "OK");
cleanup:
    if (from_tdigests)
        __td_free(from_tdigests);
    return res;
}

/**
 * Command: TDIGEST.MIN {key}
 *
 * Get minimum value from the histogram.  Will return nan if the histogram is empty.
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
    const double min = (td_size(tdigest) > 0) ? td_min(tdigest) : NAN;
    RedisModule_CloseKey(key);
    RedisModule_ReplyWithDouble(ctx, min);
    return REDISMODULE_OK;
}

/**
 * Command: TDIGEST.MAX {key}
 *
 * Get maximum value from the histogram.  Will return nan if the histogram is empty.
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
    const double max = (td_size(tdigest) > 0) ? td_max(tdigest) : NAN;
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
 * Helper method to utilize TDIGEST.RANK and TDIGEST.REVRANK common logic.
 */
static int _TDigest_Rank(RedisModuleCtx *ctx, RedisModuleString *const *argv, int argc,
                         int reverse) {
    if (argc < 3) {
        return RedisModule_WrongArity(ctx);
    }

    RedisModuleString *keyName = argv[1];
    RedisModuleKey *key = RedisModule_OpenKey(ctx, keyName, REDISMODULE_READ);

    if (_TDigest_KeyCheck(ctx, key) != REDISMODULE_OK)
        return REDISMODULE_ERR;

    const size_t n_values = argc - 2;
    double *vals = (double *)__td_calloc(n_values, sizeof(double));

    for (int i = 0; i < n_values; ++i) {
        if ((RedisModule_StringToDouble(argv[2 + i], &vals[i]) != REDISMODULE_OK) ||
            isnan(vals[i])) {
            RedisModule_CloseKey(key);
            __td_free(vals);
            return RedisModule_ReplyWithError(ctx, "ERR T-Digest: error parsing value");
        }
    }

    td_histogram_t *tdigest = RedisModule_ModuleTypeGetValue(key);
    double *ranks = (double *)__td_calloc(n_values, sizeof(double));

    const double size = td_size(tdigest);
    const double min = td_min(tdigest);
    const double max = td_max(tdigest);
    for (int i = 0; i < n_values; ++i) {
        // -2 if the sketch is empty
        if (size == 0) {
            ranks[i] = -2;
            // when value < value of the smallest observation:
            // n if reverse
            // -1 if !reverse
        } else if (vals[i] < min) {
            ranks[i] = reverse ? size : -1;
            // when value > value of the largest observation:
            // -1 if reverse
            // n if !reverse
        } else if (vals[i] > max) {
            ranks[i] = reverse ? -1 : size;
        } else {
            const double cdf_val = td_cdf(tdigest, vals[i]);
            const double cdf_val_prior_round = cdf_val * size;
            const double cdf_to_absolute =
                reverse ? round(cdf_val_prior_round) : _halfRoundDown(cdf_val_prior_round);
            ranks[i] = reverse ? round(size - cdf_to_absolute) : cdf_to_absolute;
        }
    }

    RedisModule_CloseKey(key);
    RedisModule_ReplyWithArray(ctx, n_values);
    for (int i = 0; i < n_values; ++i) {
        RedisModule_ReplyWithLongLong(ctx, (long long)ranks[i]);
    }
    __td_free(vals);
    __td_free(ranks);
    return REDISMODULE_OK;
}

/**
 * Command: TDIGEST.RANK {key} {value} [{value}...]
 *
 * Retrieve the estimated rank of value
 * (the number of observations in the sketch that are smaller than value +
 * half the number of observations that are equal to value)
 *
 * @param ctx Context in which Redis modules operate
 * @param argv Redis command arguments, as an array of strings
 * @param argc Redis command number of arguments
 * @return REDISMODULE_OK on success, or REDISMODULE_ERR  if the command failed
 */
int TDigestSketch_Rank(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    return _TDigest_Rank(ctx, argv, argc, false);
}

/**
 * Command: TDIGEST.REVRANK {key} {value} [{value}...]
 *
 * Retrieve the estimated rank of value
 * (the number of observations in the sketch that are larger than value +
 * half the number of observations that are equal to value)
 *
 * @param ctx Context in which Redis modules operate
 * @param argv Redis command arguments, as an array of strings
 * @param argc Redis command number of arguments
 * @return REDISMODULE_OK on success, or REDISMODULE_ERR  if the command failed
 */
int TDigestSketch_RevRank(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    return _TDigest_Rank(ctx, argv, argc, true);
}

static double _td_get_byrank(td_histogram_t *tdigest, const double total_observations,
                             const double input_rank) {
    const double input_p = input_rank / total_observations;
    return td_quantile(tdigest, input_p);
}

/**
 * Helper method to utilize TDIGEST.BYRANK and TDIGEST.BYREVRANK common logic.
 */
static int _TDigest_ByRank(RedisModuleCtx *ctx, RedisModuleString *const *argv, int argc,
                           int reverse) {
    if (argc < 3) {
        return RedisModule_WrongArity(ctx);
    }

    RedisModuleString *keyName = argv[1];
    RedisModuleKey *key = RedisModule_OpenKey(ctx, keyName, REDISMODULE_READ);

    if (_TDigest_KeyCheck(ctx, key) != REDISMODULE_OK)
        return REDISMODULE_ERR;

    const size_t n_values = argc - 2;
    long long *input_ranks = (long long *)__td_calloc(n_values, sizeof(long long));

    for (int i = 0; i < n_values; ++i) {
        if (RedisModule_StringToLongLong(argv[2 + i], &input_ranks[i]) != REDISMODULE_OK) {
            RedisModule_CloseKey(key);
            __td_free(input_ranks);
            return RedisModule_ReplyWithError(ctx, "ERR T-Digest: error parsing rank");
        }
        if (input_ranks[i] < 0) {
            RedisModule_CloseKey(key);
            __td_free(input_ranks);
            return RedisModule_ReplyWithError(ctx, "ERR T-Digest: rank needs to be non negative");
        }
    }

    td_histogram_t *tdigest = RedisModule_ModuleTypeGetValue(key);
    double *values = (double *)__td_calloc(n_values, sizeof(double));

    const double size = (double)td_size(tdigest);
    const double min = td_min(tdigest);
    const double max = td_max(tdigest);
    for (int i = 0; i < n_values; ++i) {
        const double input_rank = input_ranks[i];
        // Nan if the sketch is empty
        if (size == 0) {
            values[i] = NAN;
            // when rank is 0:
            // the value of the largest observation if reverse
            // the value of the smallest observation if !reverse
        } else if (input_rank == 0) {
            values[i] = reverse ? max : min;
            // when rank is larger or equal to the the sum of weights of all observations in the
            // sketch: -inf if reverse
            // +inf if !reverse
        } else if (input_rank >= size) {
            values[i] = reverse ? -INFINITY : INFINITY;
        } else {
            values[i] =
                _td_get_byrank(tdigest, size, reverse ? (size - input_rank - 1) : input_rank);
        }
    }

    RedisModule_CloseKey(key);
    RedisModule_ReplyWithArray(ctx, n_values);
    for (int i = 0; i < n_values; ++i) {
        RedisModule_ReplyWithDouble(ctx, values[i]);
    }
    __td_free(input_ranks);
    __td_free(values);
    return REDISMODULE_OK;
}

/**
 * Command: TDIGEST.BYRANK {key} {value} [{value}...]
 *
 * Retrieve an estimation of the value with the given the rank.
 *
 * @param ctx Context in which Redis modules operate
 * @param argv Redis command arguments, as an array of strings
 * @param argc Redis command number of arguments
 * @return REDISMODULE_OK on success, or REDISMODULE_ERR  if the command failed
 */
int TDigestSketch_ByRank(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    return _TDigest_ByRank(ctx, argv, argc, false);
}

/**
 * Command: TDIGEST.BYREVRANK {key} {value} [{value}...]
 *
 * Retrieve an estimation of the value with the given the reverse rank.
 *
 * @param ctx Context in which Redis modules operate
 * @param argv Redis command arguments, as an array of strings
 * @param argc Redis command number of arguments
 * @return REDISMODULE_OK on success, or REDISMODULE_ERR  if the command failed
 */
int TDigestSketch_ByRevRank(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    return _TDigest_ByRank(ctx, argv, argc, true);
}

/**
 * Command: TDIGEST.QUANTILE {key} {quantile} [{quantile2}...]
 *
 * Returns an estimate of the cutoff such that a specified fraction of the data
 * added to this TDigest would be less than or equal to the cutoff quantiles.
 * The command returns an array of results: each element of the returned array
 * populated with cutoff_1, cutoff_2, ..., cutoff_N.
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
    double *quantiles = (double *)__td_malloc(n_quantiles * sizeof(double));

    for (int i = 0; i < n_quantiles; ++i) {
        if (RedisModule_StringToDouble(argv[2 + i], &quantiles[i]) != REDISMODULE_OK) {
            RedisModule_CloseKey(key);
            __td_free(quantiles);
            return RedisModule_ReplyWithError(ctx, "ERR T-Digest: error parsing quantile");
        }
        if (quantiles[i] < 0 || quantiles[i] > 1.0) {
            RedisModule_CloseKey(key);
            __td_free(quantiles);
            return RedisModule_ReplyWithError(ctx, "ERR T-Digest: quantile should be in [0,1]");
        }
    }
    double *values = (double *)__td_malloc(n_quantiles * sizeof(double));
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
 * Command: TDIGEST.CDF {key} {value} [{value}...]
 *
 * Returns the fraction of all points added which are <= value.
 * The command returns an array of results: each element of the returned array
 * populated with fraction_1, fraction_2,..., fraction_N.
 *
 * @param ctx Context in which Redis modules operate
 * @param argv Redis command arguments, as an array of strings
 * @param argc Redis command number of arguments
 * @return REDISMODULE_OK on success, or REDISMODULE_ERR  if the command failed
 */
int TDigestSketch_Cdf(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc < 3) {
        return RedisModule_WrongArity(ctx);
    }
    RedisModuleString *keyName = argv[1];
    RedisModuleKey *key = RedisModule_OpenKey(ctx, keyName, REDISMODULE_READ);

    if (_TDigest_KeyCheck(ctx, key) != REDISMODULE_OK)
        return REDISMODULE_ERR;

    td_histogram_t *tdigest = RedisModule_ModuleTypeGetValue(key);

    const size_t n_cdfs = argc - 2;
    double *cdfs = (double *)__td_malloc(n_cdfs * sizeof(double));

    for (int i = 0; i < n_cdfs; ++i) {
        if (RedisModule_StringToDouble(argv[2 + i], &cdfs[i]) != REDISMODULE_OK) {
            RedisModule_CloseKey(key);
            __td_free(cdfs);
            return RedisModule_ReplyWithError(ctx, "ERR T-Digest: error parsing cdf");
        }
    }
    double *values = (double *)__td_malloc(n_cdfs * sizeof(double));
    for (int i = 0; i < n_cdfs; ++i) {
        values[i] = td_cdf(tdigest, cdfs[i]);
    }
    RedisModule_CloseKey(key);
    RedisModule_ReplyWithArray(ctx, n_cdfs);
    for (int i = 0; i < n_cdfs; ++i) {
        RedisModule_ReplyWithDouble(ctx, values[i]);
    }

    __td_free(cdfs);
    __td_free(values);
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
    if (low_cut_percentile >= high_cut_percentile) {
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

    RedisModule_ReplyWithMapOrArray(ctx, 9 * 2, true);
    RedisModule_ReplyWithSimpleString(ctx, "Compression");
    RedisModule_ReplyWithLongLong(ctx, tdigest->compression);
    RedisModule_ReplyWithSimpleString(ctx, "Capacity");
    RedisModule_ReplyWithLongLong(ctx, tdigest->cap);
    RedisModule_ReplyWithSimpleString(ctx, "Merged nodes");
    RedisModule_ReplyWithLongLong(ctx, tdigest->merged_nodes);
    RedisModule_ReplyWithSimpleString(ctx, "Unmerged nodes");
    RedisModule_ReplyWithLongLong(ctx, tdigest->unmerged_nodes);
    RedisModule_ReplyWithSimpleString(ctx, "Merged weight");
    RedisModule_ReplyWithLongLong(ctx, tdigest->merged_weight);
    RedisModule_ReplyWithSimpleString(ctx, "Unmerged weight");
    RedisModule_ReplyWithLongLong(ctx, tdigest->unmerged_weight);
    RedisModule_ReplyWithSimpleString(ctx, "Observations");
    RedisModule_ReplyWithLongLong(ctx, tdigest->unmerged_weight + tdigest->merged_weight);
    RedisModule_ReplyWithSimpleString(ctx, "Total compressions");
    RedisModule_ReplyWithLongLong(ctx, tdigest->total_compressions);
    RedisModule_ReplyWithSimpleString(ctx, "Memory usage");
    const size_t size_b = TDigestMemUsage(tdigest);
    RedisModule_ReplyWithLongLong(ctx, size_b);
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

static int TDigestDefrag(RedisModuleDefragCtx *ctx, RedisModuleString *key, void **value) {
    *value = defragPtr(ctx, *value);
    td_histogram_t *tdigest = *value;
    tdigest->nodes_mean = defragPtr(ctx, tdigest->nodes_mean);
    tdigest->nodes_weight = defragPtr(ctx, tdigest->nodes_weight);
}

size_t TDigestMemUsage(const void *value) {
    td_histogram_t *tdigest = (td_histogram_t *)value;
    size_t size = sizeof(tdigest);
    size += (2 * (tdigest->cap * sizeof(double)));
    return size;
}

int TDigestModule_onLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    // TODO: add option to set defaults from command line and in program
    RedisModuleTypeMethods tm = {
        .version = REDISMODULE_TYPE_METHOD_VERSION,
        .rdb_load = TDigestRdbLoad,
        .rdb_save = TDigestRdbSave,
        .aof_rewrite = RMUtil_DefaultAofRewrite,
        .mem_usage = TDigestMemUsage,
        .free = TDigestFree,
        .defrag = TDigestDefrag,
    };

    TDigestSketchType = RedisModule_CreateDataType(ctx, "TDIS-TYPE", TDIGEST_ENC_VER, &tm);
    if (TDigestSketchType == NULL) {
        return REDISMODULE_ERR;
    }

#define RegisterCommand(ctx, name, cmd, mode, acl)                                                 \
    RegisterCommandWithModesAndAcls(ctx, name, cmd, mode, acl " tdigest")

    RegisterAclCategory(ctx, "tdigest");
    RegisterCommand(ctx, "tdigest.create", TDigestSketch_Create, "write deny-oom", "write fast");
    RegisterCommand(ctx, "tdigest.add", TDigestSketch_Add, "write deny-oom", "write");
    RegisterCommand(ctx, "tdigest.reset", TDigestSketch_Reset, "write deny-oom", "write fast");
    RegisterCommand(ctx, "tdigest.merge", TDigestSketch_Merge, "write deny-oom", "write");
    RegisterCommand(ctx, "tdigest.min", TDigestSketch_Min, "readonly", "read fast");
    RegisterCommand(ctx, "tdigest.max", TDigestSketch_Max, "readonly", "read fast");
    RegisterCommand(ctx, "tdigest.quantile", TDigestSketch_Quantile, "readonly", "read fast");
    RegisterCommand(ctx, "tdigest.byrank", TDigestSketch_ByRank, "readonly", "read fast");
    RegisterCommand(ctx, "tdigest.byrevrank", TDigestSketch_ByRevRank, "readonly", "read fast");
    RegisterCommand(ctx, "tdigest.rank", TDigestSketch_Rank, "readonly", "read fast");
    RegisterCommand(ctx, "tdigest.revrank", TDigestSketch_RevRank, "readonly", "read fast");
    RegisterCommand(ctx, "tdigest.cdf", TDigestSketch_Cdf, "readonly", "read fast");
    RegisterCommand(ctx, "tdigest.trimmed_mean", TDigestSketch_TrimmedMean, "readonly", "read");
    RegisterCommand(ctx, "tdigest.info", TDigestSketch_Info, "readonly", "read fast");

#undef RegisterCommand

    if (RegisterTDigestCommandInfos(ctx) != REDISMODULE_OK)
        return REDISMODULE_ERR;

    return REDISMODULE_OK;
}
