/*
 * Copyright Redis Ltd. 2017 - present
 * Licensed under your choice of the Redis Source Available License 2.0 (RSALv2) or
 * the Server Side Public License v1 (SSPLv1).
 */

#pragma once

#include <stdarg.h>
#include <string.h>
#include "redismodule.h"
#if defined(DEBUG) || !defined(NDEBUG)
#include "readies/cetara/diag/gdb.h"
#endif

#ifdef REDISMODULE_TARGET /* Set when compiling as a module */
#define rm_malloc RedisModule_Alloc
#define rm_calloc RedisModule_Calloc
#define rm_realloc RedisModule_Realloc
#define rm_free RedisModule_Free
#else
#define rm_malloc malloc
#define rm_calloc calloc
#define rm_realloc realloc
#define rm_free free
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
        return REDISMODULE_OK;                                                                     \
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
