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

typedef enum { SEED_INPUT_INVALID, SEED_INPUT_MANUAL, SEED_INPUT_RANDOM } SeedInput;

/* Generate once after module API initialization; persist/replicate the result. */
uint64_t Seed_Generate(void);

/* Resolve an omitted SEED for a new key. Return 1 if the policy randomized it.
 * Replay keeps legacy defaults; randomized creations propagate a concrete seed. */
int Seed_ResolveDefault(RedisModuleCtx *ctx, uint64_t *seed, uint64_t maxSeed, int supportsMerge);

/* Accept "random", unsigned decimal, or 0x-prefixed hex (at most 20 characters).
 * Input need not be NUL terminated. Write value only for valid manual input.
 */
SeedInput Seed_Parse(const char *input, size_t len, uint64_t *value);

/* index points to SEED. maxSeed must be UINT32_MAX or UINT64_MAX.
 * Resolve its value and reply on invalid input. */
int Seed_ParseOption(RedisModuleCtx *ctx, RedisModuleString **argv, int argc, int index,
                     uint64_t *seed, uint64_t maxSeed);

/* Propagate a concrete seed. index points to SEED, is argc to append it,
 * or -1 to preserve the original command.
 * Caller uses RedisModule_AutoMemory for the temporary numeric string.
 */
void Seed_Replicate(RedisModuleCtx *ctx, const char *command, RedisModuleString **argv, int argc,
                    int index, uint64_t seed);
