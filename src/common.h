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

#define rm_malloc RedisModule_Alloc
#define rm_calloc RedisModule_Calloc
#define rm_realloc RedisModule_Realloc
#define rm_free RedisModule_Free

static inline void *defragPtr(RedisModuleDefragCtx *ctx, void *ptr) {
    void *tmp = RedisModule_DefragAlloc(ctx, ptr);
    return tmp ? tmp : ptr;
}

static inline int rm_vasprintf(char **restrict str, char const *restrict fmt, va_list args) {
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, fmt, args) + 1;
    *str = rm_malloc(needed);
    int res = vsprintf(*str, fmt, args_copy);
    va_end(args_copy);
    return res;
}

static int rm_asprintf(char **restrict str, char const *restrict fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int res = rm_vasprintf(str, fmt, args);
    va_end(args);
    return res;
}

static inline int SetCommandAcls(RedisModuleCtx *ctx, char const *cmd, char const *acls,
                                 char const *module) {
    RedisModuleCommand *command = RedisModule_GetCommand(ctx, cmd);
    if (command == NULL) {
        RedisModule_Log(ctx, "error", "Failed to get command %s", cmd);
        return REDISMODULE_ERR;
    }

    bool acls_empty = strcmp(acls, "") == 0;
    char const *categories = acls_empty ? module : ({
        char *c = NULL;
        rm_asprintf(&c, "%s %s", acls, module);
        c;
    });

    if (RedisModule_SetCommandACLCategories(command, categories) != REDISMODULE_OK) {
        RedisModule_Log(ctx, "error", "Failed to set ACL categories %s for command %s", categories,
                        cmd);
        return REDISMODULE_ERR;
    }
    if (!acls_empty) {
        rm_free((void*)categories);
    }
    return REDISMODULE_OK;
}

#define RegisterCommandWithModesAndAcls(ctx, cmd, f, mode, acls)                                   \
    if (RedisModule_CreateCommand(ctx, cmd, f, mode, 1, 1, 1) != REDISMODULE_OK ||                 \
        SetCommandAcls(ctx, cmd, acls, MODULE_ACL_CATEGORY_NAME) != REDISMODULE_OK) {              \
        RedisModule_Log(ctx, "error", "Failed to create command %s", cmd);                         \
        return REDISMODULE_ERR;                                                                    \
    }

#define RegisterAclCategory(ctx)                                                                   \
    if (RedisModule_AddACLCategory(ctx, MODULE_ACL_CATEGORY_NAME) != REDISMODULE_OK) {             \
        RedisModule_Log(ctx, "error", "Failed to add ACL category %s", MODULE_ACL_CATEGORY_NAME);  \
        return REDISMODULE_ERR;                                                                    \
    }
