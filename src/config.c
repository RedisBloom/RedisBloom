/*
 * Copyright Redis Ltd. 2016 - present
 * Licensed under your choice of the Redis Source Available License 2.0 (RSALv2) or
 * the Server Side Public License v1 (SSPLv1).
 */

#include <strings.h>
#include "config.h"

static int setFloatValue(const char *name, RedisModuleString *value, void *privdata,
                         RedisModuleString **err) {
    RM_ConfigFloat *config = privdata;
    double *val = &config->value;
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
    *val = new_val;
    return REDISMODULE_OK;
}
static RedisModuleString *getFloatValue(const char *name, void *privdata) {
    return RedisModule_CreateStringPrintf(NULL, "%f", ((RM_ConfigFloat *)privdata)->value);
}

static int setNumericValue(const char *name, RedisModuleString *value, void *privdata,
                           RedisModuleString **err) {
    RM_ConfigFloat *config = privdata;
    long long *val = &config->value;
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
    *val = new_val;

    if (strcasecmp(name, "cf_bucket_size") == 0) {
        rm_config.cf_initial_size.min = new_val * 2;
    } else if (strcasecmp(name, "cf_initial_size") == 0) {
        rm_config.cf_bucket_size.max = min(255, new_val / 2);
    }

    return REDISMODULE_OK;
}
static RedisModuleString *getNumericValue(const char *name, void *privdata) {
    return RedisModule_CreateStringPrintf(NULL, "%zu", *((size_t *)privdata));
}

typedef struct {
    const char *name;
    const char *defaultValue;
    unsigned int flags;
    RedisModuleConfigSetStringFunc setValue;
    RedisModuleConfigGetStringFunc getValue;
    RedisModuleConfigApplyFunc applyValue;
    void *privdata;
} RM_ConfigVar;

RM_ConfigVar rm_config_vars[] = {
    {
        .name = "bf-error-rate",
        .defaultValue = NULL,
        .flags = REDISMODULE_CONFIG_DEFAULT,
        .setValue = setFloatValue,
        .getValue = getFloatValue,
        .applyValue = NULL,
        .privdata = &rm_config.bf_error_rate,
    },
    {
        .name = "bf-initial-size",
        .defaultValue = NULL,
        .flags = REDISMODULE_CONFIG_DEFAULT,
        .setValue = setNumericValue,
        .getValue = setNumericValue,
        .applyValue = NULL,
        .privdata = &rm_config.bf_initial_size,
    },
    {
        .name = "bf-expansion-factor",
        .defaultValue = NULL,
        .flags = REDISMODULE_CONFIG_DEFAULT,
        .setValue = setNumericValue,
        .getValue = setNumericValue,
        .applyValue = NULL,
        .privdata = &rm_config.bf_expansion_factor,
    },
    {
        .name = "cf-bucket-size",
        .defaultValue = NULL,
        .flags = REDISMODULE_CONFIG_DEFAULT,
        .setValue = setNumericValue,
        .getValue = setNumericValue,
        .applyValue = NULL,
        .privdata = &rm_config.cf_bucket_size,
    },
    {
        .name = "cf-initial-size",
        .defaultValue = NULL,
        .flags = REDISMODULE_CONFIG_DEFAULT,
        .setValue = setNumericValue,
        .getValue = setNumericValue,
        .applyValue = NULL,
        .privdata = &rm_config.cf_initial_size,
    },
    {
        .name = "cf-max-iterations",
        .defaultValue = NULL,
        .flags = REDISMODULE_CONFIG_DEFAULT,
        .setValue = setNumericValue,
        .getValue = setNumericValue,
        .applyValue = NULL,
        .privdata = &rm_config.cf_max_iterations,
    },
    {
        .name = "cf-expansion-factor",
        .defaultValue = NULL,
        .flags = REDISMODULE_CONFIG_DEFAULT,
        .setValue = setNumericValue,
        .getValue = setNumericValue,
        .applyValue = NULL,
        .privdata = &rm_config.cf_expansion_factor,
    },
    {
        .name = "cf-max-expansions",
        .defaultValue = NULL,
        .flags = REDISMODULE_CONFIG_DEFAULT,
        .setValue = setNumericValue,
        .getValue = setNumericValue,
        .applyValue = NULL,
        .privdata = &rm_config.cf_max_expansions,
    },
};

int RM_RegisterConfigs(RedisModuleCtx *ctx) {
    RedisModule_Log(ctx, "notice", "Registering configuration options");

    for (int i = 0; i < sizeof rm_config_vars / sizeof *rm_config_vars; ++i) {
        RM_ConfigVar var = rm_config_vars[i];
        if (RedisModule_RegisterStringConfig(ctx, var.name, var.defaultValue, var.flags,
                                             var.getValue, var.setValue, var.applyValue,
                                             var.privdata) != REDISMODULE_OK) {
            RedisModule_Log(ctx, "warning", "Failed to register config option `%s`", var.name);
            return REDISMODULE_ERR;
        }
    }

    return REDISMODULE_OK;
}