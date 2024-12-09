/*
 * Copyright Redis Ltd. 2016 - present
 * Licensed under your choice of the Redis Source Available License 2.0 (RSALv2) or
 * the Server Side Public License v1 (SSPLv1).
 */

#pragma once

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

extern RM_Config rm_config;

// Register the module configuration options
int RM_RegisterConfigs(RedisModuleCtx *ctx);
