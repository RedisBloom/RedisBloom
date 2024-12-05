/*
 * Copyright Redis Ltd. 2016 - present
 * Licensed under your choice of the Redis Source Available License 2.0 (RSALv2) or
 * the Server Side Public License v1 (SSPLv1).
 */

#include "redismodule.h"

typedef struct {
    long long value;
    long long min;
    long long max;
} RM_ConfigNumeric;

typedef struct {
    double value;
    double min;
    double max;
} RM_ConfigFloat;

/*
 * RM_Config is a global configuration struct for the module, it can be included from each file,
 * and is initialized with user config options during module startup
 */
typedef struct {
    /*********************************
     * BLOOM FILTER CONFIG OPTIONS:  *
     *********************************/
    // Error ratio. valid range is [0.0, 1.0]. (default: 0.01)
    // TODO: consider limiting the range to [0.0, 0.25] as per CONFIG PRD
    // (https://redislabs.atlassian.net/wiki/spaces/DX/pages/3980198010/PRD+CONFIG+for+modules+arguments)
    RM_ConfigFloat bf_error_rate;
    // Initial capacity. valid range is [1, 1e9] (default: 100)
    RM_ConfigNumeric bf_initial_size;
    // Expansion factor. valid range is [0, 32768] (default: 2)
    // 0 is equivalent to NONSCALING
    RM_ConfigNumeric bf_expansion_factor;

    /*********************************
     * CUCKOO FILTER CONFIG OPTIONS: *
     *********************************/
    // Number of items in each bucket. valid range is [1, 255] (default: 2)
    RM_ConfigNumeric cf_bucket_size;
    // Initial capacity. valid range is [2*bucket_size, 1e9] (default: 1024)
    // TODO: add similar max range validation also to CF.RESERVE's capacity argument.
    RM_ConfigNumeric cf_initial_size;
    // Maximum iterations. valid range is [1, 65535] (default: 20)
    RM_ConfigNumeric cf_max_iterations;
    // expansion factor. valid range is [0, 32768] (default: 1)
    // 0 is equivalent to NONSCALING
    RM_ConfigNumeric cf_expansion_factor;
    // Maximum expansions. valid range is [1, 65535] (default: 32)
    RM_ConfigNumeric cf_max_expansions;
} RM_Config;

RM_Config rm_config = {
    .bf_error_rate =
        {
            .value = 0.01,
            .min = 0.0,
            .max = 1.0,
        },
    .bf_initial_size =
        {
            .value = 100,
            .min = 1,
            .max = 1LL << 30,
        },
    .bf_expansion_factor =
        {
            .value = 2,
            .min = 0,
            .max = 32768,
        },
    .cf_bucket_size =
        {
            .value = 2,
            .min = 1,
            .max = 255,
        },
    .cf_initial_size =
        {
            .value = 1024,
            .min = 2,
            .max = 1LL << 30,
        },
    .cf_max_iterations =
        {
            .value = 20,
            .min = 1,
            .max = 65535,
        },
    .cf_expansion_factor =
        {
            .value = 1,
            .min = 0,
            .max = 32768,
        },
    .cf_max_expansions =
        {
            .value = 32,
            .min = 1,
            .max = 65535,
        },
};

int RM_RegisterConfigs(RedisModuleCtx *ctx);
