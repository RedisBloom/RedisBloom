#include "redismodule.h"

// ===============================
// CF.RESERVE
// ===============================
static const RedisModuleCommandKeySpec CF_RESERVE_KEYSPECS[] = {
    {.notes = "Key must be a non-existing key",
     .flags = REDISMODULE_CMD_KEY_RW,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
    {0}};

static const RedisModuleCommandArg CF_RESERVE_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
    {.name = "capacity", .type = REDISMODULE_ARG_TYPE_INTEGER},
    {.name = "bucketsize",
     .type = REDISMODULE_ARG_TYPE_BLOCK,
     .flags = REDISMODULE_CMD_ARG_OPTIONAL,
     .subargs = (RedisModuleCommandArg[]){{.name = "CAPACITY", .type = REDISMODULE_ARG_TYPE_STRING},
                                          {.name = "VALUE", .type = REDISMODULE_ARG_TYPE_STRING},
                                          {0}}},
    {.name = "maxiterations",
     .type = REDISMODULE_ARG_TYPE_BLOCK,
     .flags = REDISMODULE_CMD_ARG_OPTIONAL,
     .subargs =
         (RedisModuleCommandArg[]){{.name = "MAXITERATIONS", .type = REDISMODULE_ARG_TYPE_STRING},
                                   {.name = "VALUE", .type = REDISMODULE_ARG_TYPE_STRING},
                                   {0}}},
    {.name = "expansion",
     .type = REDISMODULE_ARG_TYPE_BLOCK,
     .flags = REDISMODULE_CMD_ARG_OPTIONAL,
     .subargs =
         (RedisModuleCommandArg[]){{.name = "EXPANSION", .type = REDISMODULE_ARG_TYPE_STRING},
                                   {.name = "VALUE", .type = REDISMODULE_ARG_TYPE_STRING},
                                   {0}}},
    {0}};

static const RedisModuleCommandInfo CF_RESERVE_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary = "Creates a new Cuckoo Filter",
    .complexity = "O(1)",
    .since = "1.0.0",
    .arity = -3,
    .key_specs = (RedisModuleCommandKeySpec *)CF_RESERVE_KEYSPECS,
    .args = (RedisModuleCommandArg *)CF_RESERVE_ARGS,
};

// ===============================
// CF.DEL
// ===============================
static const RedisModuleCommandKeySpec CF_DEL_KEYSPECS[] = {
    {.notes = "is key name for a cuckoo filter.",
     .flags = REDISMODULE_CMD_KEY_RW,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
    {0}};

static const RedisModuleCommandArg CF_DEL_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
    {.name = "item", .type = REDISMODULE_ARG_TYPE_STRING},
    {0}};

static const RedisModuleCommandInfo CF_DEL_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary =
        "Deletes an item once from the filter. If the item exists only once, it will be removed "
        "from the filter. If the item was added multiple times, it will still be present.",
    .complexity = "O(k), where k is the number of sub-filters",
    .since = "1.0.0",
    .arity = 3,
    .key_specs = (RedisModuleCommandKeySpec *)CF_DEL_KEYSPECS,
    .args = (RedisModuleCommandArg *)CF_DEL_ARGS,
};

// ===============================
// CF.EXISTS
// ===============================
static const RedisModuleCommandKeySpec CF_EXISTS_KEYSPECS[] = {
    {.notes = "is key name for a cuckoo filter.",
     .flags = REDISMODULE_CMD_KEY_RO,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
    {0}};

static const RedisModuleCommandArg CF_EXISTS_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
    {.name = "item", .type = REDISMODULE_ARG_TYPE_STRING},
    {0}};

static const RedisModuleCommandInfo CF_EXISTS_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary = "Determines whether a given item was added to a cuckoo filter.",
    .complexity = "O(k), where k is the number of sub-filters",
    .since = "1.0.0",
    .arity = 3,
    .key_specs = (RedisModuleCommandKeySpec *)CF_EXISTS_KEYSPECS,
    .args = (RedisModuleCommandArg *)CF_EXISTS_ARGS,
};

// ===============================
// CF.INFO
// ===============================
static const RedisModuleCommandKeySpec CF_INFO_KEYSPECS[] = {
    {.notes = "is key name for a cuckoo filter.",
     .flags = REDISMODULE_CMD_KEY_RO,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
    {0}};

static const RedisModuleCommandArg CF_INFO_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0}, {0}};

static const RedisModuleCommandInfo CF_INFO_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary = "Returns information about a cuckoo filter.",
    .complexity = "O(1)",
    .since = "1.0.0",
    .arity = 1,
    .key_specs = (RedisModuleCommandKeySpec *)CF_INFO_KEYSPECS,
    .args = (RedisModuleCommandArg *)CF_INFO_ARGS,
};

// ===============================
// CF.INSERT
// ===============================
static const RedisModuleCommandKeySpec CF_INSERT_KEYSPECS[] = {
    {.notes =
         "is key name for a cuckoo filter. If key does not exist - a new cuckoo filter is created.",
     .flags = REDISMODULE_CMD_KEY_RW,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
    {0}};

static const RedisModuleCommandArg CF_INSERT_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
    {.name = "capacity",
     .type = REDISMODULE_ARG_TYPE_BLOCK,
     .flags = REDISMODULE_CMD_ARG_OPTIONAL,
     .subargs =
         (RedisModuleCommandArg[]){
             {.name = "capacity", .type = REDISMODULE_ARG_TYPE_PURE_TOKEN, .token = "CAPACITY"},
             {.name = "capacity", .type = REDISMODULE_ARG_TYPE_INTEGER},
             {0}}},
    {.name = "NOCREATE",
     .type = REDISMODULE_ARG_TYPE_PURE_TOKEN,
     .flags = REDISMODULE_CMD_ARG_OPTIONAL,
     .token = "NOCREATE"},
    {.name = "items", .type = REDISMODULE_ARG_TYPE_PURE_TOKEN, .token = "ITEMS"},
    {.name = "item", .type = REDISMODULE_ARG_TYPE_STRING, .flags = REDISMODULE_CMD_ARG_MULTIPLE},
    {0}};

static const RedisModuleCommandInfo CF_INSERT_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary = "Adds one or more items to a cuckoo filter, allowing the filter to be created with "
               "a custom capacity if it does not exist yet. This command is similar to CF.ADD, "
               "except that more than one item can be added and capacity can be specified.",
    .complexity = "O(n * (k + i)), where n is the number of items, k is the number of "
                  "sub-filters and i is maxIterations",
    .since = "1.0.0",
    .arity = -4,
    .key_specs = (RedisModuleCommandKeySpec *)CF_INSERT_KEYSPECS,
    .args = (RedisModuleCommandArg *)CF_INSERT_ARGS,
};

// ===============================
// CF.INSERTNX
// ===============================
static const RedisModuleCommandKeySpec CF_INSERTNX_KEYSPECS[] = {
    {.notes =
         "is key name for a cuckoo filter. If key does not exist - a new cuckoo filter is created.",
     .flags = REDISMODULE_CMD_KEY_RW,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
    {0}};

static const RedisModuleCommandArg CF_INSERTNX_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
    {.name = "capacity",
     .type = REDISMODULE_ARG_TYPE_BLOCK,
     .flags = REDISMODULE_CMD_ARG_OPTIONAL,
     .subargs =
         (RedisModuleCommandArg[]){
             {.name = "capacity", .type = REDISMODULE_ARG_TYPE_PURE_TOKEN, .token = "CAPACITY"},
             {.name = "capacity", .type = REDISMODULE_ARG_TYPE_INTEGER},
             {0}}},
    {.name = "NOCREATE",
     .type = REDISMODULE_ARG_TYPE_PURE_TOKEN,
     .flags = REDISMODULE_CMD_ARG_OPTIONAL,
     .token = "NOCREATE"},
    {.name = "items", .type = REDISMODULE_ARG_TYPE_PURE_TOKEN, .token = "ITEMS"},
    {.name = "item", .type = REDISMODULE_ARG_TYPE_STRING, .flags = REDISMODULE_CMD_ARG_MULTIPLE},
    {0}};

static const RedisModuleCommandInfo CF_INSERTNX_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary = "Adds one or more items to a cuckoo filter if they did not exist previously, "
               "allowing the filter to be created with a custom capacity if it does not exist yet."
               "sub-filters and i is maxIterations",
    .since = "1.0.0",
    .arity = -4,
    .key_specs = (RedisModuleCommandKeySpec *)CF_INSERT_KEYSPECS,
    .args = (RedisModuleCommandArg *)CF_INSERT_ARGS,
};

// ===============================
// CF.LOADCHUNK
// ===============================
static const RedisModuleCommandKeySpec CF_LOADCHUNK_KEYSPECS[] = {
    {.notes = "is key name for a cuckoo filter.",
     .flags = REDISMODULE_CMD_KEY_RW,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
    {0}};

static const RedisModuleCommandArg CF_LOADCHUNK_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
    {.name = "iterator", .type = REDISMODULE_ARG_TYPE_INTEGER},
    {.name = "data", .type = REDISMODULE_ARG_TYPE_STRING},
    {0}};

static const RedisModuleCommandInfo CF_LOADCHUNK_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary = "Restores a cuckoo filter previously saved using CF.SCANDUMP.",
    .complexity = "O(n), where n is the capacity",
    .since = "1.0.0",
    .arity = 4,
    .key_specs = (RedisModuleCommandKeySpec *)CF_LOADCHUNK_KEYSPECS,
    .args = (RedisModuleCommandArg *)CF_LOADCHUNK_ARGS,
};

int RegisterCFCommandInfos(RedisModuleCtx *ctx) {
    RedisModuleCommand *reserve_cmd = RedisModule_GetCommand(ctx, "cf.reserve");
    if (!reserve_cmd)
        return REDISMODULE_ERR;
    if (RedisModule_SetCommandInfo(reserve_cmd, &CF_RESERVE_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModuleCommand *del_cmd = RedisModule_GetCommand(ctx, "cf.del");
    if (!del_cmd)
        return REDISMODULE_ERR;
    if (RedisModule_SetCommandInfo(del_cmd, &CF_DEL_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModuleCommand *exists_cmd = RedisModule_GetCommand(ctx, "cf.exists");
    if (!exists_cmd)
        return REDISMODULE_ERR;
    if (RedisModule_SetCommandInfo(exists_cmd, &CF_EXISTS_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModuleCommand *info_cmd = RedisModule_GetCommand(ctx, "cf.info");
    if (!info_cmd)
        return REDISMODULE_ERR;
    if (RedisModule_SetCommandInfo(info_cmd, &CF_INFO_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModuleCommand *insert_cmd = RedisModule_GetCommand(ctx, "cf.insert");
    if (!insert_cmd)
        return REDISMODULE_ERR;
    if (RedisModule_SetCommandInfo(insert_cmd, &CF_INSERT_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModuleCommand *insertnx_cmd = RedisModule_GetCommand(ctx, "cf.insertnx");
    if (!insertnx_cmd)
        return REDISMODULE_ERR;
    if (RedisModule_SetCommandInfo(insertnx_cmd, &CF_INSERTNX_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModuleCommand *loadchunk_cmd = RedisModule_GetCommand(ctx, "cf.loadchunk");
    if (!loadchunk_cmd)
        return REDISMODULE_ERR;
    if (RedisModule_SetCommandInfo(loadchunk_cmd, &CF_LOADCHUNK_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
