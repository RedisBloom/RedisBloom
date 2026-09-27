/*
 * Copyright (c) 2006-Present, Redis Ltd.
 * All rights reserved.
 *
 * Licensed under your choice of (a) the Redis Source Available License 2.0
 * (RSALv2); or (b) the Server Side Public License v1 (SSPLv1); or (c) the
 * GNU Affero General Public License v3 (AGPLv3).
 */

#include "seed.h"
#include "rmutil/util.h"
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* Call once when creating a randomly seeded key, after module API initialization.
 * Persist and replicate the returned value, not another request for randomness.
 */
uint64_t Seed_Generate(void) {
    uint64_t seed;
    RedisModule_GetRandomBytes((unsigned char *)&seed, sizeof(seed));
    return seed;
}

/* Parse the value following SEED: case-insensitive "random", unsigned decimal,
 * or 0x-prefixed hexadecimal. Accept at most 20 characters, with no sign or
 * whitespace. Input need not be NUL
 * terminated. Write value only for manual input; do not generate randomness
 * here. An omitted SEED option is handled by the command's legacy path.
 */
SeedInput Seed_Parse(const char *input, size_t len, uint64_t *value) {
    if (input == NULL || value == NULL || len == 0 || len > 20) {
        return SEED_INPUT_INVALID;
    }
    if (len == 6 && strncasecmp(input, "random", len) == 0) {
        return SEED_INPUT_RANDOM;
    }

    /* strtoull accepts signs and leading whitespace; the command does not. */
    if (input[0] < '0' || input[0] > '9') {
        return SEED_INPUT_INVALID;
    }
    char buffer[21], *end;
    memcpy(buffer, input, len);
    buffer[len] = '\0';
    /* Explicit bases keep leading-zero decimal values from becoming octal. */
    int base = len >= 2 && input[0] == '0' && (input[1] == 'x' || input[1] == 'X') ? 16 : 10;
    errno = 0;
    unsigned long long parsed = strtoull(buffer, &end, base);
    if (errno == ERANGE || end != buffer + len || parsed > UINT64_MAX) {
        return SEED_INPUT_INVALID;
    }
    *value = parsed;
    return SEED_INPUT_MANUAL;
}

/* index points to SEED. Resolve its value and reply on invalid input. */
int Seed_ParseOption(RedisModuleCtx *ctx, RedisModuleString **argv, int argc,
                                  int index, uint64_t *seed) {
    if (index + 1 == argc ||
        RMUtil_ArgIndex("SEED", argv + index + 1, argc - index - 1) != -1) {
        RedisModule_ReplyWithError(ctx, "ERR expected one SEED value");
        return REDISMODULE_ERR;
    }
    size_t len;
    const char *input = RedisModule_StringPtrLen(argv[index + 1], &len);
    SeedInput kind = Seed_Parse(input, len, seed);
    if (kind == SEED_INPUT_INVALID) {
        RedisModule_ReplyWithError(
            ctx, "ERR invalid seed: expected random, unsigned decimal or 0x hexadecimal");
        return REDISMODULE_ERR;
    }
    if (kind == SEED_INPUT_RANDOM) *seed = Seed_Generate();
    return REDISMODULE_OK;
}

/* Propagate a concrete seed so replay never generates a new random value.
 * index is the SEED token position, or -1 when the option was omitted.
 * Caller uses RedisModule_AutoMemory for the temporary numeric string.
 */
void Seed_Replicate(RedisModuleCtx *ctx, const char *command,
                                 RedisModuleString **argv, int argc, int index, uint64_t seed) {
    if (index == -1) {
        RedisModule_ReplicateVerbatim(ctx);
        return;
    }
    RedisModuleString **args = RedisModule_PoolAlloc(ctx, sizeof(*args) * (argc - 1));
    memcpy(args, argv + 1, sizeof(*args) * (argc - 1));
    args[index] = RedisModule_CreateStringPrintf(ctx, "%" PRIu64, seed);
    RedisModule_Replicate(ctx, command, "v", args, (size_t)(argc - 1));
}
