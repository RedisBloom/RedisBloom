#include "redismodule.h"

// ===============================
// BF.ADD
// ===============================
static const RedisModuleCommandKeySpec BF_ADD_KEYSPECS[] = {
    {.notes = "is key name for a Bloom filter to add the item to.",
     .flags = REDISMODULE_CMD_KEY_RW,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 1, .keystep = 0, .limit = 0}},
    {0}};

static const RedisModuleCommandArg BF_ADD_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
    {.name = "item", .type = REDISMODULE_ARG_TYPE_STRING},
    {0}};

static const RedisModuleCommandInfo BF_ADD_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary = "Adds an item to a Bloom filter.",
    .complexity = "O(k), where k is the number of hash functions used by the last sub-filter",
    .since = "1.0.0",
    .arity = 3,
    .key_specs = (RedisModuleCommandKeySpec *)BF_ADD_KEYSPECS,
    .args = (RedisModuleCommandArg *)BF_ADD_ARGS,
};

// ===============================
// BF.EXISTS
// ===============================
static const RedisModuleCommandKeySpec BF_EXISTS_KEYSPECS[] = {
    {.notes = "is key name for a Bloom filter to check the item in.",
     .flags = REDISMODULE_CMD_KEY_RO,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 1, .keystep = 0, .limit = 0}},
    {0}};

static const RedisModuleCommandArg BF_EXISTS_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
    {.name = "item", .type = REDISMODULE_ARG_TYPE_STRING},
    {0}};

static const RedisModuleCommandInfo BF_EXISTS_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary = "Checks whether an item exists in a Bloom Filter",
    .complexity = "O(k), where k is the number of hash functions used by the last sub-filter",
    .since = "1.0.0",
    .arity = 3,
    .key_specs = (RedisModuleCommandKeySpec *)BF_EXISTS_KEYSPECS,
    .args = (RedisModuleCommandArg *)BF_EXISTS_ARGS,
};

// ===============================
// BF.INFO
// ===============================
static const RedisModuleCommandKeySpec BF_INFO_KEYSPECS[] = {
    {.notes = "is key name for a Bloom filter.",
     .flags = REDISMODULE_CMD_KEY_RO,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 1, .keystep = 0, .limit = 0}},
    {0}};

static const RedisModuleCommandArg BF_INFO_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
    {.name = "single_value",
     .type = REDISMODULE_ARG_TYPE_ONEOF,
     .flags = REDISMODULE_CMD_ARG_OPTIONAL,
     .subargs =
         (RedisModuleCommandArg[]){
             {.name = "CAPACITY", .type = REDISMODULE_ARG_TYPE_PURE_TOKEN, .token = "CAPACITY"},
             {.name = "SIZE", .type = REDISMODULE_ARG_TYPE_PURE_TOKEN, .token = "SIZE"},
             {.name = "FILTERS", .type = REDISMODULE_ARG_TYPE_PURE_TOKEN, .token = "FILTERS"},
             {.name = "ITEMS", .type = REDISMODULE_ARG_TYPE_PURE_TOKEN, .token = "ITEMS"},
             {.name = "EXPANSION", .type = REDISMODULE_ARG_TYPE_PURE_TOKEN, .token = "EXPANSION"},
             {0},
         }},
    {0}};

static const RedisModuleCommandInfo BF_INFO_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary = "Returns information about a Bloom Filter",
    .complexity = "O(1)",
    .since = "1.0.0",
    .arity = -2,
    .key_specs = (RedisModuleCommandKeySpec *)BF_INFO_KEYSPECS,
    .args = (RedisModuleCommandArg *)BF_INFO_ARGS,
};

// ===============================
// BF.CARD
// ===============================
static const RedisModuleCommandKeySpec BF_CARD_KEYSPECS[] = {
    {.notes = "is key name for a Bloom filter.",
     .flags = REDISMODULE_CMD_KEY_RO,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 1, .keystep = 0, .limit = 0}},
    {0}};

static const RedisModuleCommandArg BF_CARD_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0}, {0}};

static const RedisModuleCommandInfo BF_CARD_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary = "Returns the cardinality of a Bloom filter",
    .complexity = "O(1)",
    .since = "2.4.4",
    .arity = 2,
    .key_specs = (RedisModuleCommandKeySpec *)BF_CARD_KEYSPECS,
    .args = (RedisModuleCommandArg *)BF_CARD_ARGS,
};

// ===============================
// BF.INSERT
// ===============================
static const RedisModuleCommandKeySpec BF_INSERT_KEYSPECS[] = {
    {.notes = "is key name for a Bloom filter to insert the item to.",
     .flags = REDISMODULE_CMD_KEY_RW,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 1, .keystep = 0, .limit = 0}},
    {0}};

static const RedisModuleCommandArg BF_INSERT_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
    {.name = "capacity",
     .type = REDISMODULE_ARG_TYPE_BLOCK,
     .flags = REDISMODULE_CMD_ARG_OPTIONAL,
     .token = "CAPACITY",
     .subargs =
         (RedisModuleCommandArg[]){{.name = "capacity", .type = REDISMODULE_ARG_TYPE_INTEGER},
                                   {0}}},
    {.name = "error",
     .type = REDISMODULE_ARG_TYPE_BLOCK,
     .flags = REDISMODULE_CMD_ARG_OPTIONAL,
     .token = "ERROR",
     .subargs =
         (RedisModuleCommandArg[]){{.name = "error", .type = REDISMODULE_ARG_TYPE_DOUBLE}, {0}}},
    {.name = "expansion",
     .type = REDISMODULE_ARG_TYPE_BLOCK,
     .flags = REDISMODULE_CMD_ARG_OPTIONAL,
     .token = "EXPANSION",
     .subargs =
         (RedisModuleCommandArg[]){{.name = "expansion", .type = REDISMODULE_ARG_TYPE_INTEGER},
                                   {0}}},
    {
        .name = "nocreate",
        .type = REDISMODULE_ARG_TYPE_PURE_TOKEN,
        .flags = REDISMODULE_CMD_ARG_OPTIONAL,
        .token = "NOCREATE",
    },
    {
        .name = "nonscaling",
        .type = REDISMODULE_ARG_TYPE_PURE_TOKEN,
        .flags = REDISMODULE_CMD_ARG_OPTIONAL,
        .token = "NONSCALING",
    },
    {.name = "items", .type = REDISMODULE_ARG_TYPE_PURE_TOKEN, .token = "ITEMS"},
    {.name = "item", .type = REDISMODULE_ARG_TYPE_STRING, .flags = REDISMODULE_CMD_ARG_MULTIPLE},
    {0}};
static const RedisModuleCommandInfo BF_INSERT_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary =
        "Adds one or more items to a Bloom Filter. A filter will be created if it does not exist",
    .complexity = "O(k * n), where k is the number of hash functions and n is the number of items",
    .since = "1.0.0",
    .arity = -4,
    .key_specs = (RedisModuleCommandKeySpec *)BF_INSERT_KEYSPECS,
    .args = (RedisModuleCommandArg *)BF_INSERT_ARGS,
};

// ===============================
// BF.LOADCHUNK
// ===============================
static const RedisModuleCommandKeySpec BF_LOADCHUNK_KEYSPECS[] = {
    {.notes = "is key name for a Bloom filter to load the chunk to.",
     .flags = REDISMODULE_CMD_KEY_RW,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 1, .keystep = 0, .limit = 0}},
    {0}};

static const RedisModuleCommandArg BF_LOADCHUNK_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
    {.name = "iterator", .type = REDISMODULE_ARG_TYPE_INTEGER},
    {.name = "data", .type = REDISMODULE_ARG_TYPE_STRING},
    {0}};

static const RedisModuleCommandInfo BF_LOADCHUNK_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary = "Restores a filter previously saved using SCANDUMP",
    .complexity = "O(n), where n is the capacity",
    .since = "1.0.0",
    .arity = 4,
    .key_specs = (RedisModuleCommandKeySpec *)BF_LOADCHUNK_KEYSPECS,
    .args = (RedisModuleCommandArg *)BF_LOADCHUNK_ARGS,
};

// ===============================
// BF.MADD
// ===============================
static const RedisModuleCommandKeySpec BF_MADD_KEYSPECS[] = {
    {.notes = "is key name for a Bloom filter to add the items to.",
     .flags = REDISMODULE_CMD_KEY_RW,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 1, .keystep = 0, .limit = 0}},
    {0}};

static const RedisModuleCommandArg BF_MADD_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
    {.name = "item", .type = REDISMODULE_ARG_TYPE_STRING, .flags = REDISMODULE_CMD_ARG_MULTIPLE},
    {0}};
static const RedisModuleCommandInfo BF_MADD_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary = "Adds one or more items to a Bloom Filter. A filter will be created if it does not exist",
    .complexity = "O(k * n), where k is the number of hash functions and n is the number of items",
    .since = "1.0.0",
    .arity = -3,
    .key_specs = (RedisModuleCommandKeySpec *)BF_MADD_KEYSPECS,
    .args = (RedisModuleCommandArg *)BF_MADD_ARGS,
};


int RegisterBFCommandInfos(RedisModuleCtx *ctx) {
    RedisModuleCommand *cmd_add = RedisModule_GetCommand(ctx, "bf.add");
    if (!cmd_add)
        return REDISMODULE_ERR;
    if (RedisModule_SetCommandInfo(cmd_add, &BF_ADD_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModuleCommand *cmd_exists = RedisModule_GetCommand(ctx, "bf.exists");
    if (!cmd_exists)
        return REDISMODULE_ERR;
    if (RedisModule_SetCommandInfo(cmd_exists, &BF_EXISTS_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModuleCommand *cmd_info = RedisModule_GetCommand(ctx, "bf.info");
    if (!cmd_info)
        return REDISMODULE_ERR;
    if (RedisModule_SetCommandInfo(cmd_info, &BF_INFO_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModuleCommand *cmd_card = RedisModule_GetCommand(ctx, "bf.card");
    if (!cmd_card)
        return REDISMODULE_ERR;
    if (RedisModule_SetCommandInfo(cmd_card, &BF_CARD_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModuleCommand *cmd_insert = RedisModule_GetCommand(ctx, "bf.insert");
    if (!cmd_insert)
        return REDISMODULE_ERR;
    if (RedisModule_SetCommandInfo(cmd_insert, &BF_INSERT_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModuleCommand *cmd_loadchunk = RedisModule_GetCommand(ctx, "bf.loadchunk");
    if (!cmd_loadchunk)
        return REDISMODULE_ERR;
    if (RedisModule_SetCommandInfo(cmd_loadchunk, &BF_LOADCHUNK_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModuleCommand *cmd_madd = RedisModule_GetCommand(ctx, "bf.madd");
    if (!cmd_madd)
        return REDISMODULE_ERR;
    if (RedisModule_SetCommandInfo(cmd_madd, &BF_MADD_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
