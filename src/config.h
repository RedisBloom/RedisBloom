/*
 * Copyright Redis Ltd. 2016 - present
 * Licensed under your choice of the Redis Source Available License 2.0 (RSALv2) or
 * the Server Side Public License v1 (SSPLv1).
 */

#pragma once

#include "redismodule.h"
#include <string.h>
#include <strings.h>

typedef enum {
    bf_error_rate,
    bf_initial_size,
    bf_expansion_factor,
    cf_bucket_size,
    cf_initial_size,
    cf_max_iterations,
    cf_expansion_factor,
    cf_max_expansions,

    RM_CONFIG_COUNT,
} RM_ConfigOption;

#define BF_ERROR_RATE_LEGACY "ERROR_RATE"
#define BF_INITIAL_SIZE_LEGACY "INITIAL_SIZE"
#define CF_MAX_EXPANSIONS_LEGACY "CF_MAX_EXPANSIONS"

static const char *RM_ConfigOptionToString(RM_ConfigOption option) {
    static const char *RM_ConfigOptionStrings[] = {
        [bf_error_rate] = "bf-error-rate",
        [bf_initial_size] = "bf-initial-size",
        [bf_expansion_factor] = "bf-expansion-factor",
        [cf_bucket_size] = "cf-bucket-size",
        [cf_initial_size] = "cf-initial-size",
        [cf_max_iterations] = "cf-max-iterations",
        [cf_expansion_factor] = "cf-expansion-factor",
        [cf_max_expansions] = "cf-max-expansions",
    };
    if (0 <= option && option < RM_CONFIG_COUNT) {
        return RM_ConfigOptionStrings[option];
    }
    return NULL;
}

static inline int RM_ConfigStrCaseCmp(const char *s, RM_ConfigOption config) {
    return strcasecmp(s, RM_ConfigOptionToString(config));
}
static inline int RM_ConfigRMStrCaseCmp(const RedisModuleString *s, RM_ConfigOption config) {
    size_t len;
    const char *str = RedisModule_StringPtrLen(s, &len);
    const char *name = RM_ConfigOptionToString(config);
    if (len != strlen(name)) {
        return 1;
    }
    return strncasecmp(str, name, len);
}

typedef struct {
    long long value;
    long long min;
    long long max;
} RM_ConfigInteger;

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
    // Error ratio.
    RM_ConfigFloat bf_error_rate;
    // Initial capacity.
    RM_ConfigInteger bf_initial_size;
    // Expansion factor. 0 is equivalent to NONSCALING
    RM_ConfigInteger bf_expansion_factor;

    /*********************************
     * CUCKOO FILTER CONFIG OPTIONS: *
     *********************************/
    // Number of items in each bucket.
    RM_ConfigInteger cf_bucket_size;
    // Initial capacity.
    RM_ConfigInteger cf_initial_size;
    // Maximum iterations.
    RM_ConfigInteger cf_max_iterations;
    // expansion factor. 0 is equivalent to NONSCALING
    RM_ConfigInteger cf_expansion_factor;
    // Maximum expansions.
    RM_ConfigInteger cf_max_expansions;
} RM_Config;

extern RM_Config rm_config;

static inline int isIntegerConfigValid(long long config, RM_ConfigInteger params) {
    return config >= params.min && config <= params.max;
}
static inline int isFloatConfigValid(double config, RM_ConfigFloat params) {
    return config >= params.min && config <= params.max;
}

#define isConfigValid(config, params)                                                              \
    _Generic((config),                                                                             \
        uint16_t: isIntegerConfigValid,                                                            \
        long long: isIntegerConfigValid,                                                           \
        double: isFloatConfigValid)(config, params)

// Register the module configuration options
int RM_RegisterConfigs(RedisModuleCtx *ctx);
