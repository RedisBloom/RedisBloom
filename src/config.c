/*
 * Copyright (c) 2006-Present, Redis Ltd.
 * All rights reserved.
 *
 * Licensed under your choice of (a) the Redis Source Available License 2.0
 * (RSALv2); or (b) the Server Side Public License v1 (SSPLv1); or (c) the
 * GNU Affero General Public License v3 (AGPLv3).
 */

#include <strings.h>
#include "config.h"

// Default configuration values.
RM_Config rm_config = {
    // A value greater than BF_ERROR_RATE_CAP is treated as BF_ERROR_RATE_CAP
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
            .min = 4,
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
            .max = 65536,
        },
};

static int setFloatValue(const char *name, RedisModuleString *value, void *privdata,
                         RedisModuleString **err) {
    RM_ConfigFloat *config = privdata;
    double new_val;
    if (RedisModule_StringToDouble(value, &new_val) != REDISMODULE_OK) {
        *err = RedisModule_CreateStringPrintf(NULL, "Invalid value for `%s`", name);
        return REDISMODULE_ERR;
    }
    if (!isFloatConfigValid(new_val, *config)) {
        *err = RedisModule_CreateStringPrintf(NULL, "Value for `%s` must be in the range (%f, %f)",
                                              name, config->min, config->max);
        return REDISMODULE_ERR;
    }

    if (!RM_ConfigStrCaseCmp(name, bf_error_rate) && new_val > BF_ERROR_RATE_CAP) {
        new_val = BF_ERROR_RATE_CAP;
        value = RedisModule_CreateStringFromDouble(NULL, new_val);
    }

    config->value = new_val;
    RedisModule_FreeString(NULL, config->str_value);
    config->str_value = RedisModule_HoldString(NULL, value);
    return REDISMODULE_OK;
}
static RedisModuleString *getFloatValue(const char *name, void *privdata) {
    return ((RM_ConfigFloat *)privdata)->str_value;
}

static int setIntegerValue(const char *name, RedisModuleString *value, void *privdata,
                           RedisModuleString **err) {
    RM_ConfigInteger *config = privdata;
    long long new_val;
    if (RedisModule_StringToLongLong(value, &new_val) != REDISMODULE_OK) {
        *err = RedisModule_CreateStringPrintf(NULL, "Invalid value for `%s`", name);
        return REDISMODULE_ERR;
    }
    if (!isIntegerConfigValid(new_val, *config)) {
        *err =
            RedisModule_CreateStringPrintf(NULL, "Value for `%s` must be in the range [%lld, %lld]",
                                           name, config->min, config->max);
        return REDISMODULE_ERR;
    }
    config->value = new_val;
    RedisModule_FreeString(NULL, config->str_value);
    config->str_value = RedisModule_HoldString(NULL, value);

    if (!RM_ConfigStrCaseCmp(name, cf_bucket_size)) {
        rm_config.cf_initial_size.min = new_val * 2;
    } else if (!RM_ConfigStrCaseCmp(name, cf_initial_size)) {
        rm_config.cf_bucket_size.max = (new_val / 2 < 255) ? new_val / 2 : 255;
    }

    return REDISMODULE_OK;
}
static RedisModuleString *getIntegerValue(const char *name, void *privdata) {
    return ((RM_ConfigInteger *)privdata)->str_value;
}

#define getValue(config)                                                                           \
    _Generic(rm_config.config.value, long long: getIntegerValue, double: getFloatValue)

#define setValue(config)                                                                           \
    _Generic(rm_config.config.value, long long: setIntegerValue, double: setFloatValue)

#define RM_createStringFromNumber(num)                                                             \
    _Generic(num,                                                                                  \
        long long: RedisModule_CreateStringFromLongLong,                                           \
        double: RedisModule_CreateStringFromDouble)(NULL, num)

#define registerConfigVar(config)                                                                  \
    do {                                                                                           \
        const char *name = RM_ConfigOptionToString(config);                                        \
        rm_config.config.str_value = RM_createStringFromNumber(rm_config.config.value);            \
        const char *default_val = RedisModule_StringPtrLen(rm_config.config.str_value, NULL);      \
        if (RedisModule_RegisterStringConfig(                                                      \
                ctx, name, default_val, REDISMODULE_CONFIG_UNPREFIXED, getValue(config),           \
                setValue(config), NULL, &rm_config.config) != REDISMODULE_OK) {                    \
            RedisModule_Log(ctx, "warning", "Failed to register config option `%s`", name);        \
            return REDISMODULE_ERR;                                                                \
        }                                                                                          \
        RedisModule_Log(ctx, "notice", "\t{ %-*s:%*s }", 20, name, 10, default_val);               \
    } while (0)

int RM_RegisterConfigs(RedisModuleCtx *ctx) {
    RedisModule_Log(ctx, "notice", "Registering configuration options: [");
    registerConfigVar(bf_error_rate);
    registerConfigVar(bf_initial_size);
    registerConfigVar(bf_expansion_factor);
    registerConfigVar(cf_bucket_size);
    registerConfigVar(cf_initial_size);
    registerConfigVar(cf_max_iterations);
    registerConfigVar(cf_expansion_factor);
    registerConfigVar(cf_max_expansions);
    RedisModule_Log(ctx, "notice", "]");

    return REDISMODULE_OK;
}
