#include "redismodule.h"
// ===============================
// CMS.INCRBY
// ===============================
static const RedisModuleCommandKeySpec CMS_INCRBY_KEYSPECS[] = {
    {.notes = "the name of the sketch",
     .flags = REDISMODULE_CMD_KEY_RW,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
    {0}};

static const RedisModuleCommandArg CMS_INCRBY_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
    {
        .name = "items",
        .type = REDISMODULE_ARG_TYPE_BLOCK,
        .flags = REDISMODULE_CMD_ARG_MULTIPLE,
        .subargs =
            (RedisModuleCommandArg[]){{.name = "item", .type = REDISMODULE_ARG_TYPE_STRING},
                                      {.name = "increment", .type = REDISMODULE_ARG_TYPE_INTEGER},
                                      {0}},
    },
    {0}};

static const RedisModuleCommandInfo CMS_INCRBY_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary =
        "Increases the count of item by increment. Multiple items can be increased with one call.",
    .complexity = "O(n) where n is the number of items",
    .since = "2.0.0",
    .arity = -4,
    .key_specs = (RedisModuleCommandKeySpec *)CMS_INCRBY_KEYSPECS,
    .args = (RedisModuleCommandArg *)CMS_INCRBY_ARGS,
};

// ===============================
// CMS.INFO
// ===============================
static const RedisModuleCommandKeySpec CMS_INFO_KEYSPECS[] = {
    {.notes = "the name of the sketch",
     .flags = REDISMODULE_CMD_KEY_RO,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
    {0}};

static const RedisModuleCommandArg CMS_INFO_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0}, {0}};

static const RedisModuleCommandInfo CMS_INFO_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary = "Returns width, depth and total count of the sketch",
    .complexity = "O(1)",
    .since = "2.0.0",
    .arity = 2,
    .key_specs = (RedisModuleCommandKeySpec *)CMS_INFO_KEYSPECS,
    .args = (RedisModuleCommandArg *)CMS_INFO_ARGS,
};

int RegisterCMSCommandInfos(RedisModuleCtx *ctx) {
    RedisModuleCommand *cmd_incrby = RedisModule_GetCommand(ctx, "CMS.INCRBY");
    if (!cmd_incrby) {
        return REDISMODULE_ERR;
    }
    if (RedisModule_SetCommandInfo(cmd_incrby, &CMS_INCRBY_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModuleCommand *cmd_info = RedisModule_GetCommand(ctx, "CMS.INFO");
    if (!cmd_info) {
        return REDISMODULE_ERR;
    }
    if (RedisModule_SetCommandInfo(cmd_info, &CMS_INFO_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }
    return REDISMODULE_OK;
}
