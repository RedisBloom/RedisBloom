/*
 * Copyright Redis Ltd. 2017 - present
 * Licensed under your choice of the Redis Source Available License 2.0 (RSALv2) or
 * the Server Side Public License v1 (SSPLv1).
 */

#pragma once

#include <stdarg.h>
#include <string.h>
#include "redismodule.h"
#include "rmutil/util.h"
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

static inline int rm_vasprintf(char **restrict str, const char *restrict fmt, va_list args) {
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, fmt, args) + 1;
    *str = rm_malloc(needed);
    int res = vsprintf(*str, fmt, args_copy);
    va_end(args_copy);
    return res;
}

static int rm_asprintf(char **restrict str, const char *restrict fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int res = rm_vasprintf(str, fmt, args);
    va_end(args);
    return res;
}

#define RegisterAclCategory(ctx)                                                                   \
    do {                                                                                           \
        if (RedisModule_AddACLCategory(ctx, MODULE_ACL_CATEGORY_NAME) != REDISMODULE_OK) {         \
            RedisModule_Log(ctx, "error", "Failed to add ACL category %s",                         \
                            MODULE_ACL_CATEGORY_NAME);                                             \
            return REDISMODULE_ERR;                                                                \
        }                                                                                          \
    } while (0)

#define SetCommandAcls(ctx, cmd, acls)                                                             \
    do {                                                                                           \
        RedisModuleCommand *command = RedisModule_GetCommand(ctx, cmd);                            \
        if (command == NULL) {                                                                     \
            RedisModule_Log(ctx, "error", "Failed to get command %s", cmd);                        \
            return REDISMODULE_ERR;                                                                \
        }                                                                                          \
        char *categories = NULL;                                                                   \
        if (!strcmp(acls, "")) {                                                                   \
            categories = MODULE_ACL_CATEGORY_NAME;                                                 \
        } else {                                                                                   \
            rm_asprintf(&categories, "%s %s", acls, MODULE_ACL_CATEGORY_NAME);                     \
        }                                                                                          \
        if (RedisModule_SetCommandACLCategories(command, categories) != REDISMODULE_OK) {          \
            RedisModule_Log(ctx, "error", "Failed to set ACL categories %s for command %s",        \
                            categories, cmd);                                                      \
            return REDISMODULE_ERR;                                                                \
        }                                                                                          \
        if (strcmp(acls, "")) {                                                                    \
            rm_free(categories);                                                                   \
        }                                                                                          \
    } while (0)

#define RegisterCommandWithModesAndAcls(ctx, cmd, f, mode, acls)                                   \
    do {                                                                                           \
        __rmutil_register_cmd(ctx, cmd, f, mode);                                                  \
        SetCommandAcls(ctx, cmd, acls);                                                            \
    } while (0)
