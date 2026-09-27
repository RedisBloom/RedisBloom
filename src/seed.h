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
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "redismodule.h"

typedef enum {
    SEED_INPUT_INVALID,
    SEED_INPUT_MANUAL,
    SEED_INPUT_RANDOM
} SeedInput;

/* Call once when creating a randomly seeded key, after module API initialization.
 * Persist and replicate the returned value, not another request for randomness.
 */
static inline uint64_t Seed_Generate(void) {
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
static inline SeedInput Seed_Parse(const char *input, size_t len, uint64_t *value) {
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
