#include "redismodule.h"

// ===============================
// TOPK.ADD
// ===============================
static const RedisModuleCommandKeySpec TOPK_ADD_KEYSPECS[] = {
    {.notes = "the name of the sketch where items are added",
     .flags = REDISMODULE_CMD_KEY_RW,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
    {0}};

static const RedisModuleCommandArg TOPK_ADD_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
    {.name = "item", .type = REDISMODULE_ARG_TYPE_STRING, .flags = REDISMODULE_CMD_ARG_MULTIPLE},
    {0}};

static const RedisModuleCommandInfo TOPK_ADD_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary = "Adds an item to a Top-k sketch. Multiple items can be added at the same time. If "
               "an item enters the Top-K sketch, the item that is expelled (if any) is returned",
    .complexity = "O(n * k) where n is the number of items and k is the depth",
    .since = "2.0.0",
    .arity = -3,
    .key_specs = (RedisModuleCommandKeySpec *)TOPK_ADD_KEYSPECS,
    .args = (RedisModuleCommandArg *)TOPK_ADD_ARGS,
};

// ===============================
// TOPK.COUNT
// ===============================
static const RedisModuleCommandKeySpec TOPK_COUNT_KEYSPECS[] = {
    {.notes = "the name of the sketch where items are to be counted",
     .flags = REDISMODULE_CMD_KEY_RO,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
    {0}};

static const RedisModuleCommandArg TOPK_COUNT_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
    {.name = "item", .type = REDISMODULE_ARG_TYPE_STRING, .flags = REDISMODULE_CMD_ARG_MULTIPLE},
    {0}};

static const RedisModuleCommandInfo TOPK_COUNT_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary = "Return the count for one or more items are in a sketch",
    .complexity = "O(n) where n is the number of items",
    .since = "2.0.0",
    .history =
        (RedisModuleCommandHistoryEntry[]){
            {"2.4.0", "As of Bloom version 2.4, this command is regarded as deprecated. The count "
                      "value is not a representative of the number of appearances of an item"},
            {0}},
    .arity = -3,
    .key_specs = (RedisModuleCommandKeySpec *)TOPK_COUNT_KEYSPECS,
    .args = (RedisModuleCommandArg *)TOPK_COUNT_ARGS,
};

int RegisterTopKCommandInfos(RedisModuleCtx *ctx) {
    RedisModuleCommand *cmd_add = RedisModule_GetCommand(ctx, "TOPK.ADD");
    if (!cmd_add) {
        return REDISMODULE_ERR;
    }
    if (RedisModule_SetCommandInfo(cmd_add, &TOPK_ADD_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModuleCommand *cmd_count = RedisModule_GetCommand(ctx, "TOPK.COUNT");
    if (!cmd_count) {
        return REDISMODULE_ERR;
    }
    if (RedisModule_SetCommandInfo(cmd_count, &TOPK_COUNT_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
