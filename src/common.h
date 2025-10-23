/*
 * Copyright (c) 2006-Present, Redis Ltd.
 * All rights reserved.
 *
 * Licensed under your choice of (a) the Redis Source Available License 2.0
 * (RSALv2); or (b) the Server Side Public License v1 (SSPLv1); or (c) the
 * GNU Affero General Public License v3 (AGPLv3).
 */

#pragma once

#if defined(__GNUC__)
#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)
#elif _MSC_VER
#define likely(x) (x)
#define unlikely(x) (x)
#endif

#include "redismodule.h"
#if defined(DEBUG) || !defined(NDEBUG)
#include "readies/cetara/diag/gdb.h"
#endif

static inline void *defragPtr(RedisModuleDefragCtx *ctx, void *ptr) {
    void *tmp = RedisModule_DefragAlloc(ctx, ptr);
    return tmp ? tmp : ptr;
}

#define SetCommandAcls(ctx, cmd, acls)                                                             \
    do {                                                                                           \
        RedisModuleCommand *command = RedisModule_GetCommand(ctx, cmd);                            \
        if (command == NULL) {                                                                     \
            RedisModule_Log(ctx, "error", "Failed to get command %s", cmd);                        \
            return REDISMODULE_ERR;                                                                \
        }                                                                                          \
        if (RedisModule_SetCommandACLCategories(command, acls) != REDISMODULE_OK) {                \
            RedisModule_Log(ctx, "error", "Failed to set ACL categories %s for command %s", acls,  \
                            cmd);                                                                  \
            return REDISMODULE_ERR;                                                                \
        }                                                                                          \
    } while (0)

#define RegisterCommandWithModesAndAcls(ctx, cmd, f, mode, acls)                                   \
    do {                                                                                           \
        if (RedisModule_CreateCommand(ctx, cmd, f, mode, 1, 1, 1) != REDISMODULE_OK) {             \
            RedisModule_Log(ctx, "error", "Failed to create command %s", cmd);                     \
            return REDISMODULE_ERR;                                                                \
        }                                                                                          \
        SetCommandAcls(ctx, cmd, acls);                                                            \
    } while (0)

#define RegisterAclCategory(ctx, module)                                                           \
    if (RedisModule_AddACLCategory(ctx, module) != REDISMODULE_OK) {                               \
        RedisModule_Log(ctx, "error", "Failed to add ACL category %s", module);                    \
        return REDISMODULE_ERR;                                                                    \
    }
