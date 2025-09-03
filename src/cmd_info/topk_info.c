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

// ===============================
// TOPK.INCRBY
// ===============================
static const RedisModuleCommandKeySpec TOPK_INCRBY_KEYSPECS[] = {
    {.notes = "the name of the sketch",
     .flags = REDISMODULE_CMD_KEY_RW,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
    {0}};

static const RedisModuleCommandArg TOPK_INCRBY_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
    {
        .name = "item_increment",
        .type = REDISMODULE_ARG_TYPE_BLOCK,
        .flags = REDISMODULE_CMD_ARG_MULTIPLE,
        .subargs =
            (RedisModuleCommandArg[]){{.name = "item", .type = REDISMODULE_ARG_TYPE_STRING},
                                      {.name = "increment", .type = REDISMODULE_ARG_TYPE_INTEGER},
                                      {0}},
    },
    {0}};

static const RedisModuleCommandInfo TOPK_INCRBY_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary = "Increases the count of one or more items by increment",
    .complexity =
        "O(n * k * incr) where n is the number of items, k is the depth and incr is the increment",
    .since = "2.0.0",
    .arity = -4,
    .key_specs = (RedisModuleCommandKeySpec *)TOPK_INCRBY_KEYSPECS,
    .args = (RedisModuleCommandArg *)TOPK_INCRBY_ARGS,
};

// ===============================
// TOPK.INFO
// ===============================
static const RedisModuleCommandKeySpec TOPK_INFO_KEYSPECS[] = {
    {.notes = "the name of the sketch",
     .flags = REDISMODULE_CMD_KEY_RO,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
    {0}};

static const RedisModuleCommandArg TOPK_INFO_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0}, {0}};

static const RedisModuleCommandInfo TOPK_INFO_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary =
        "Returns number of required items (k), width, depth, and decay values of a given sketch",
    .complexity = "O(1)",
    .since = "2.0.0",
    .arity = 2,
    .key_specs = (RedisModuleCommandKeySpec *)TOPK_INFO_KEYSPECS,
    .args = (RedisModuleCommandArg *)TOPK_INFO_ARGS,
};

// ===============================
// TOPK.LIST
// ===============================
static const RedisModuleCommandKeySpec TOPK_LIST_KEYSPECS[] = {
    {.notes = "the name of the sketch",
     .flags = REDISMODULE_CMD_KEY_RO,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
    {0}};

static const RedisModuleCommandArg TOPK_LIST_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
    {.name = "withcount",
     .type = REDISMODULE_ARG_TYPE_PURE_TOKEN,
     .token = "WITHCOUNT",
     .flags = REDISMODULE_CMD_ARG_OPTIONAL},
    {0}};

static const RedisModuleCommandInfo TOPK_LIST_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary = "Return the full list of items in Top-K sketch",
    .complexity = "O(k*log(k)) where k is the value of top-k",
    .since = "2.0.0",
    .arity = -2,
    .key_specs = (RedisModuleCommandKeySpec *)TOPK_LIST_KEYSPECS,
    .args = (RedisModuleCommandArg *)TOPK_LIST_ARGS,
};

// ===============================
// TOPK.QUERY
// ===============================
static const RedisModuleCommandKeySpec TOPK_QUERY_KEYSPECS[] = {
    {.notes = "the name of the sketch",
     .flags = REDISMODULE_CMD_KEY_RO,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
    {0}};

static const RedisModuleCommandArg TOPK_QUERY_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
    {.name = "item", .type = REDISMODULE_ARG_TYPE_STRING, .flags = REDISMODULE_CMD_ARG_MULTIPLE},
    {0}};

static const RedisModuleCommandInfo TOPK_QUERY_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary = "Checks whether one or more items are one of the Top-K items",
    .complexity = "O(n) where n is the number of items",
    .since = "2.0.0",
    .arity = -3,
    .key_specs = (RedisModuleCommandKeySpec *)TOPK_QUERY_KEYSPECS,
    .args = (RedisModuleCommandArg *)TOPK_QUERY_ARGS,
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

    RedisModuleCommand *cmd_incrby = RedisModule_GetCommand(ctx, "TOPK.INCRBY");
    if (!cmd_incrby) {
        return REDISMODULE_ERR;
    }
    if (RedisModule_SetCommandInfo(cmd_incrby, &TOPK_INCRBY_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModuleCommand *cmd_info = RedisModule_GetCommand(ctx, "TOPK.INFO");
    if (!cmd_info) {
        return REDISMODULE_ERR;
    }
    if (RedisModule_SetCommandInfo(cmd_info, &TOPK_INFO_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModuleCommand *cmd_list = RedisModule_GetCommand(ctx, "TOPK.LIST");
    if (!cmd_list) {
        return REDISMODULE_ERR;
    }
    if (RedisModule_SetCommandInfo(cmd_list, &TOPK_LIST_INFO) == REDISMODULE_ERR) {
        printf("TOPK.LIST command info set error\n");
        return REDISMODULE_ERR;
    }

    RedisModuleCommand *cmd_query = RedisModule_GetCommand(ctx, "TOPK.QUERY");
    if (!cmd_query) {
        return REDISMODULE_ERR;
    }
    if (RedisModule_SetCommandInfo(cmd_query, &TOPK_QUERY_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
