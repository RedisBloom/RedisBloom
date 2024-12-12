/*
 * Copyright Redis Ltd. 2016 - present
 * Licensed under your choice of the Redis Source Available License 2.0 (RSALv2) or
 * the Server Side Public License v1 (SSPLv1).
 */

#include <strings.h>
#include "config.h"

// Default configuration values.
// All values provided by CONFIG PRD
// (https://redislabs.atlassian.net/wiki/spaces/DX/pages/3980198010/PRD+CONFIG+for+modules+arguments)
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
            .max = 65535,
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
    if (new_val < config->min || new_val > config->max) {
        *err = RedisModule_CreateStringPrintf(NULL, "Value for `%s` must be in the range [%f, %f]",
                                              name, config->min, config->max);
        return REDISMODULE_ERR;
    }
    config->value = new_val;
    return REDISMODULE_OK;
}
static RedisModuleString *getFloatValue(const char *name, void *privdata) {
    return RedisModule_CreateStringPrintf(NULL, "%f", ((RM_ConfigFloat *)privdata)->value);
}

static int setNumericValue(const char *name, RedisModuleString *value, void *privdata,
                           RedisModuleString **err) {
    RM_ConfigNumeric *config = privdata;
    long long new_val;
    if (RedisModule_StringToLongLong(value, &new_val) != REDISMODULE_OK) {
        *err = RedisModule_CreateStringPrintf(NULL, "Invalid value for `%s`", name);
        return REDISMODULE_ERR;
    }
    if (new_val < config->min || new_val > config->max) {
        *err =
            RedisModule_CreateStringPrintf(NULL, "Value for `%s` must be in the range [%lld, %lld]",
                                           name, config->min, config->max);
        return REDISMODULE_ERR;
    }
    config->value = new_val;

    if (strcasecmp(name, "cf-bucket-size") == 0) {
        rm_config.cf_initial_size.min = new_val * 2;
    } else if (strcasecmp(name, "cf-initial-size") == 0) {
        rm_config.cf_bucket_size.max = new_val / 2 < 255 ? new_val / 2 : 255;
    }

    return REDISMODULE_OK;
}
static RedisModuleString *getNumericValue(const char *name, void *privdata) {
    return RedisModule_CreateStringPrintf(NULL, "%zu", *((size_t *)privdata));
}

typedef struct {
    const char *name;
    const char *defVal;
    RedisModuleConfigSetStringFunc setValue;
    RedisModuleConfigGetStringFunc getValue;
    void *privdata;
} RM_ConfigVar;

RM_ConfigVar rm_config_vars[] = {
    {
        .name = "bf-error-rate",
        .defVal = "0.01",
        .setValue = setFloatValue,
        .getValue = getFloatValue,
        .privdata = &rm_config.bf_error_rate,
    },
    {
        .name = "bf-initial-size",
        .defVal = "100",
        .setValue = setNumericValue,
        .getValue = getNumericValue,
        .privdata = &rm_config.bf_initial_size,
    },
    {
        .name = "bf-expansion-factor",
        .defVal = "2",
        .setValue = setNumericValue,
        .getValue = getNumericValue,
        .privdata = &rm_config.bf_expansion_factor,
    },
    {
        .name = "cf-bucket-size",
        .defVal = "2",
        .setValue = setNumericValue,
        .getValue = getNumericValue,
        .privdata = &rm_config.cf_bucket_size,
    },
    {
        .name = "cf-initial-size",
        .defVal = "1024",
        .setValue = setNumericValue,
        .getValue = getNumericValue,
        .privdata = &rm_config.cf_initial_size,
    },
    {
        .name = "cf-max-iterations",
        .defVal = "20",
        .setValue = setNumericValue,
        .getValue = getNumericValue,
        .privdata = &rm_config.cf_max_iterations,
    },
    {
        .name = "cf-expansion-factor",
        .defVal = "1",
        .setValue = setNumericValue,
        .getValue = getNumericValue,
        .privdata = &rm_config.cf_expansion_factor,
    },
    {
        .name = "cf-max-expansions",
        .defVal = "32",
        .setValue = setNumericValue,
        .getValue = getNumericValue,
        .privdata = &rm_config.cf_max_expansions,
    },
};

int RegisterStringConfig(RedisModuleCtx *ctx, RM_ConfigVar var) {
    return RedisModule_RegisterStringConfig(ctx, var.name, var.defVal,
                                            REDISMODULE_CONFIG_UNPREFIXED, var.getValue,
                                            var.setValue, NULL, var.privdata);
}

int RM_RegisterConfigs(RedisModuleCtx *ctx) {
    RedisModule_Log(ctx, "notice", "Registering configuration options");

    for (int i = 0; i < sizeof rm_config_vars / sizeof *rm_config_vars; ++i) {
        RM_ConfigVar var = rm_config_vars[i];
        if (RegisterStringConfig(ctx, var) != REDISMODULE_OK) {
            RedisModule_Log(ctx, "warning", "Failed to register config option `%s`", var.name);
            return REDISMODULE_ERR;
        }
    }

    return RedisModule_LoadConfigs(ctx);
}
