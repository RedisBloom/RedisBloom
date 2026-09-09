/*
 * Copyright (c) 2006-Present, Redis Ltd.
 * All rights reserved.
 *
 * Licensed under your choice of (a) the Redis Source Available License 2.0
 * (RSALv2); or (b) the Server Side Public License v1 (SSPLv1); or (c) the
 * GNU Affero General Public License v3 (AGPLv3).
 */

#pragma once
#include <stddef.h>
#include <stdint.h>
#include "redismodule.h"

/* Zero is the legacy mapping. Version 1 uses XXH3-64 with a shared seed.
 * Never change the meaning of an existing version. */
typedef struct {
    uint64_t version;
    uint64_t seed;
} RBHashConfig;
extern RBHashConfig RBHash_Default;
int RBHash_RegisterConfig(RedisModuleCtx *ctx);
int RBHash_InitCompatibility(RedisModuleCtx *ctx);
int RBHash_Compatible(const RBHashConfig *a, const RBHashConfig *b);
uint64_t RBHash_Hash(const RBHashConfig *config, const void *data, size_t len, uint64_t domain);
void RBHash_Save(RedisModuleIO *io, const RBHashConfig *config);
int RBHash_Load(RedisModuleIO *io, RBHashConfig *config, int has_config);
/* Portable 16-byte representation, including format version. */
void RBHash_Encode(unsigned char out[16], const RBHashConfig *config);
int RBHash_Decode(const unsigned char in[16], RBHashConfig *config);

void RBHash_PropagateConfig(RedisModuleCtx *ctx, const RedisModuleString *key);
