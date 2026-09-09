/*
 * Copyright (c) 2006-Present, Redis Ltd.
 * All rights reserved.
 *
 * Licensed under your choice of (a) the Redis Source Available License 2.0
 * (RSALv2); or (b) the Server Side Public License v1 (SSPLv1); or (c) the
 * GNU Affero General Public License v3 (AGPLv3).
 */

#include "hash_config.h"
#define XXH_INLINE_ALL
#include "xxhash/xxhash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

RBHashConfig RBHash_Default = {0};
static RedisModuleString *seed_config;

static RedisModuleString *getSeed(const char *name, void *privdata) {
    (void)name;
    (void)privdata;
    return seed_config;
}

static int setSeed(const char *name, RedisModuleString *value, void *privdata,
                   RedisModuleString **err) {
    (void)name;
    (void)privdata;
    size_t len;
    const char *s = RedisModule_StringPtrLen(value, &len);
    RBHashConfig config = {0};
    if (len != 0) {
        if (len != 16)
            goto invalid;
        config.version = 1;
        for (size_t i = 0; i < len; i++) {
            unsigned digit;
            if (s[i] >= '0' && s[i] <= '9')
                digit = s[i] - '0';
            else if (s[i] >= 'a' && s[i] <= 'f')
                digit = s[i] - 'a' + 10;
            else if (s[i] >= 'A' && s[i] <= 'F')
                digit = s[i] - 'A' + 10;
            else
                goto invalid;
            config.seed = (config.seed << 4) | digit;
        }
    }
    RedisModule_FreeString(NULL, seed_config);
    seed_config = RedisModule_HoldString(NULL, value);
    RBHash_Default = config;
    return REDISMODULE_OK;
invalid: {
    const char *message = "bf-hash-seed must be empty (legacy) or exactly 16 hexadecimal digits";
    *err = RedisModule_CreateString(NULL, message, strlen(message));
    return REDISMODULE_ERR;
}
}

/* AOF creation records must identify their mapping even before an AOF rewrite.
 * This assertion has no data side effects and is not sent to replicas: their
 * transport handshake already enforces the same immutable configuration. */
static int checkAofConfig(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc != 3)
        return RedisModule_WrongArity(ctx);
    size_t len;
    const char *data = RedisModule_StringPtrLen(argv[2], &len);
    RBHashConfig config;
    if (len != 16 || RBHash_Decode((const unsigned char *)data, &config) != REDISMODULE_OK) {
        /* AOF replay intentionally discards command errors. Refuse startup
         * instead of continuing to create sketches with a different mapping. */
        if (RedisModule_GetContextFlags(ctx) & REDISMODULE_CTX_FLAGS_LOADING) {
            RedisModule_Log(ctx, "warning",
                            "Incompatible RedisBloom AOF hash configuration; restore the original "
                            "bf-hash-seed");
            exit(EXIT_FAILURE);
        }
        return RedisModule_ReplyWithError(ctx,
                                          "ERR incompatible RedisBloom AOF hash configuration");
    }
    return RedisModule_ReplyWithSimpleString(ctx, "OK");
}

void RBHash_PropagateConfig(RedisModuleCtx *ctx, const RedisModuleString *key) {
    unsigned char data[16];
    RBHash_Encode(data, &RBHash_Default);
    RedisModule_Replicate(ctx, "bf._hashcheck", "Rsb", key, data, sizeof(data));
}

int RBHash_RegisterConfig(RedisModuleCtx *ctx) {
    if (RedisModule_CreateCommand(ctx, "bf._hashcheck", checkAofConfig, "readonly fast", 1, 1, 1) !=
        REDISMODULE_OK)
        return REDISMODULE_ERR;
    seed_config = RedisModule_CreateString(NULL, "", 0);
    return RedisModule_RegisterStringConfig(
        ctx, "bf-hash-seed", "",
        REDISMODULE_CONFIG_UNPREFIXED | REDISMODULE_CONFIG_IMMUTABLE | REDISMODULE_CONFIG_SENSITIVE,
        getSeed, setSeed, NULL, NULL);
}

int RBHash_InitCompatibility(RedisModuleCtx *ctx) {
    if (!RBHash_Default.version)
        return REDISMODULE_OK;
    int (*registerCompatibility)(RedisModuleCtx *, const char *, size_t) = NULL;
    if (RedisModule_GetApi("RedisModule_SetReplicationCompatibility", &registerCompatibility) !=
            REDISMODULE_OK ||
        !registerCompatibility) {
        RedisModule_Log(ctx, "warning",
                        "bf-hash-seed requires Redis module replication compatibility support");
        return REDISMODULE_ERR;
    }
    /* Canonical input; never send the raw seed on a replication handshake. */
    char descriptor[80];
    int len = snprintf(descriptor, sizeof(descriptor), "redisbloom:xxh3-64:v1:%016llx",
                       (unsigned long long)RBHash_Default.seed);
    if (registerCompatibility(ctx, descriptor, (size_t)len) != REDISMODULE_OK) {
        RedisModule_Log(ctx, "warning",
                        "bf-hash-seed requires loading RedisBloom at server startup");
        return REDISMODULE_ERR;
    }
    return REDISMODULE_OK;
}

int RBHash_Compatible(const RBHashConfig *a, const RBHashConfig *b) {
    return a->version == b->version && a->seed == b->seed;
}

uint64_t RBHash_Hash(const RBHashConfig *config, const void *data, size_t len, uint64_t domain) {
    /* An odd multiplier gives distinct derived seeds for distinct domains. */
    return XXH3_64bits_withSeed(data, len, config->seed + domain * UINT64_C(0x9e3779b97f4a7c15));
}

void RBHash_Save(RedisModuleIO *io, const RBHashConfig *config) {
    RedisModule_SaveUnsigned(io, config->version);
    RedisModule_SaveUnsigned(io, config->seed);
}

int RBHash_Load(RedisModuleIO *io, RBHashConfig *config, int has_config) {
    *config = (RBHashConfig){0};
    if (has_config) {
        config->version = RedisModule_LoadUnsigned(io);
        if (RedisModule_IsIOError(io))
            return REDISMODULE_ERR;
        config->seed = RedisModule_LoadUnsigned(io);
        if (RedisModule_IsIOError(io))
            return REDISMODULE_ERR;
    }
    if (!RBHash_Compatible(config, &RBHash_Default)) {
        RedisModule_LogIOError(io, "warning",
                               "Incompatible RedisBloom hash configuration; rebuild sketches "
                               "before changing bf-hash-seed");
        return REDISMODULE_ERR;
    }
    return REDISMODULE_OK;
}

void RBHash_Encode(unsigned char out[16], const RBHashConfig *config) {
    for (unsigned i = 0; i < 8; i++) {
        out[i] = config->version >> (8 * i);
        out[8 + i] = config->seed >> (8 * i);
    }
}

int RBHash_Decode(const unsigned char in[16], RBHashConfig *config) {
    *config = (RBHashConfig){0};
    for (unsigned i = 0; i < 8; i++) {
        config->version |= (uint64_t)in[i] << (8 * i);
        config->seed |= (uint64_t)in[8 + i] << (8 * i);
    }
    return RBHash_Compatible(config, &RBHash_Default) ? REDISMODULE_OK : REDISMODULE_ERR;
}
