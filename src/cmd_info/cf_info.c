#include "redismodule.h"

// ===============================
// CF.ADD key item
// ===============================
static const RedisModuleCommandKeySpec CF_ADD_KEYSPECS[] = {
    {.notes = "",
     .flags = REDISMODULE_CMD_KEY_RW,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
    {0}};

static const RedisModuleCommandArg CF_ADD_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
    {.name = "item", .type = REDISMODULE_ARG_TYPE_STRING},
    {0}};

static const RedisModuleCommandInfo CF_ADD_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary = "Adds an item to a Cuckoo Filter",
    .complexity = "O(k + i), where k is the number of sub-filters and i is maxIterations",
    .since = "1.0.0",
    .arity = 3,
    .key_specs = (RedisModuleCommandKeySpec *)CF_ADD_KEYSPECS,
    .args = (RedisModuleCommandArg *)CF_ADD_ARGS,
};

// ===============================
// CF.ADDNX key item
// ===============================
static const RedisModuleCommandKeySpec CF_ADDNX_KEYSPECS[] = {
    {.notes = "This command is slower than CF.ADD because it first checks whether the item exists. "
              "Since CF.EXISTS can result in false positive, CF.ADDNX may not add an item because "
              "it is supposedly already exist, which may be wrong.",
     .flags = REDISMODULE_CMD_KEY_RW,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
    {0}};

static const RedisModuleCommandArg CF_ADDNX_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
    {.name = "item", .type = REDISMODULE_ARG_TYPE_STRING},
    {0}};

static const RedisModuleCommandInfo CF_ADDNX_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary = "Adds an item to a Cuckoo Filter if the item did not exist previously.",
    .complexity = "O(k + i), where k is the number of sub-filters and i is maxIterations",
    .since = "1.0.0",
    .arity = 3,
    .key_specs = (RedisModuleCommandKeySpec *)CF_ADDNX_KEYSPECS,
    .args = (RedisModuleCommandArg *)CF_ADDNX_ARGS,
};

// ===============================
// CF.COUNT key item
// ===============================
static const RedisModuleCommandKeySpec CF_COUNT_KEYSPECS[] = {
    {.notes = "",
     .flags = REDISMODULE_CMD_KEY_RO,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
    {0}};

static const RedisModuleCommandArg CF_COUNT_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
    {.name = "item", .type = REDISMODULE_ARG_TYPE_STRING},
    {0}};

static const RedisModuleCommandInfo CF_COUNT_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary = "Return the number of times an item might be in a Cuckoo Filter",
    .complexity = "O(k), where k is the number of sub-filters",
    .since = "1.0.0",
    .arity = 3,
    .key_specs = (RedisModuleCommandKeySpec *)CF_COUNT_KEYSPECS,
    .args = (RedisModuleCommandArg *)CF_COUNT_ARGS,
};

// ===============================
// CF.DEL key item
// ===============================
static const RedisModuleCommandKeySpec CF_DEL_KEYSPECS[] = {
    {.notes = "",
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
    .summary = "Deletes an item from a Cuckoo Filter",
    .complexity = "O(k), where k is the number of sub-filters",
    .since = "1.0.0",
    .arity = 3,
    .key_specs = (RedisModuleCommandKeySpec *)CF_DEL_KEYSPECS,
    .args = (RedisModuleCommandArg *)CF_DEL_ARGS,
};

// ===============================
// CF.EXISTS key item
// ===============================
static const RedisModuleCommandKeySpec CF_EXISTS_KEYSPECS[] = {
    {.notes = "",
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
    .summary = "Checks whether one or more items exist in a Cuckoo Filter",
    .complexity = "O(k), where k is the number of sub-filters",
    .since = "1.0.0",
    .arity = 3,
    .key_specs = (RedisModuleCommandKeySpec *)CF_EXISTS_KEYSPECS,
    .args = (RedisModuleCommandArg *)CF_EXISTS_ARGS,
};

// ===============================
// CF.INFO key
// ===============================
static const RedisModuleCommandKeySpec CF_INFO_KEYSPECS[] = {
    {.notes = "",
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
    .summary = "Returns information about a Cuckoo Filter",
    .complexity = "O(1)",
    .since = "1.0.0",
    .arity = 2,
    .key_specs = (RedisModuleCommandKeySpec *)CF_INFO_KEYSPECS,
    .args = (RedisModuleCommandArg *)CF_INFO_ARGS,
};

// ===============================
// CF.INSERT key [CAPACITY capacity] [NOCREATE] ITEMS item [item ...]
// ===============================
static const RedisModuleCommandKeySpec CF_INSERT_KEYSPECS[] = {
    {.notes = "",
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
    {.name = "nocreate",
     .type = REDISMODULE_ARG_TYPE_PURE_TOKEN,
     .flags = REDISMODULE_CMD_ARG_OPTIONAL,
     .token = "NOCREATE"},
    {.name = "items", .type = REDISMODULE_ARG_TYPE_PURE_TOKEN, .token = "ITEMS"},
    {.name = "item", .type = REDISMODULE_ARG_TYPE_STRING, .flags = REDISMODULE_CMD_ARG_MULTIPLE},
    {0}};

static const RedisModuleCommandInfo CF_INSERT_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary =
        "Adds one or more items to a Cuckoo Filter. A filter will be created if it does not exist",
    .complexity = "O(n * (k + i)), where n is the number of items, k is the number of sub-filters "
                  "and i is maxIterations",
    .since = "1.0.0",
    .arity = -4,
    .key_specs = (RedisModuleCommandKeySpec *)CF_INSERT_KEYSPECS,
    .args = (RedisModuleCommandArg *)CF_INSERT_ARGS,
};

// ===============================
// CF.INSERTNX key [CAPACITY capacity] [NOCREATE] ITEMS item [item ...]
// ===============================
static const RedisModuleCommandKeySpec CF_INSERTNX_KEYSPECS[] = {
    {.notes = "This command is slower than CF.INSERT because it first checks whether each item "
              "exists. Since CF.EXISTS can result in false positive, CF.INSERTNX may not add an "
              "item because it is supposedly already exist, which may be wrong.",
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
    {.name = "nocreate",
     .type = REDISMODULE_ARG_TYPE_PURE_TOKEN,
     .flags = REDISMODULE_CMD_ARG_OPTIONAL,
     .token = "NOCREATE"},
    {.name = "items", .type = REDISMODULE_ARG_TYPE_PURE_TOKEN, .token = "ITEMS"},
    {.name = "item", .type = REDISMODULE_ARG_TYPE_STRING, .flags = REDISMODULE_CMD_ARG_MULTIPLE},
    {0}};

static const RedisModuleCommandInfo CF_INSERTNX_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary = "Adds one or more items to a Cuckoo Filter if the items did not exist previously. A "
               "filter will be created if it does not exist",
    .complexity = "O(n * (k + i)), where n is the number of items, k is the number of sub-filters "
                  "and i is maxIterations",
    .since = "1.0.0",
    .arity = -4,
    .key_specs = (RedisModuleCommandKeySpec *)CF_INSERT_KEYSPECS,
    .args = (RedisModuleCommandArg *)CF_INSERT_ARGS,
};

// ===============================
// CF.LOADCHUNK key iterator data
// ===============================
static const RedisModuleCommandKeySpec CF_LOADCHUNK_KEYSPECS[] = {
    {.notes = "This command overwrites the cuckoo filter stored under key. Make sure that the "
              "cuckoo filter is not changed between invocations.",
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
    .summary = "Restores a filter previously saved using SCANDUMP",
    .complexity = "O(n), where n is the capacity",
    .since = "1.0.0",
    .arity = 4,
    .key_specs = (RedisModuleCommandKeySpec *)CF_LOADCHUNK_KEYSPECS,
    .args = (RedisModuleCommandArg *)CF_LOADCHUNK_ARGS,
};

// ===============================
// CF.MEXISTS key item [item ...]
// ===============================
static const RedisModuleCommandKeySpec CF_MEXISTS_KEYSPECS[] = {
    {.notes = "",
     .flags = REDISMODULE_CMD_KEY_RO,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
    {0}};

static const RedisModuleCommandArg CF_MEXISTS_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
    {.name = "item", .type = REDISMODULE_ARG_TYPE_STRING, .flags = REDISMODULE_CMD_ARG_MULTIPLE},
    {0}};

static const RedisModuleCommandInfo CF_MEXISTS_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary = "Checks whether one or more items exist in a Cuckoo Filter",
    .complexity = "O(k * n), where k is the number of sub-filters and n is the number of items",
    .since = "1.0.0",
    .arity = -3,
    .key_specs = (RedisModuleCommandKeySpec *)CF_MEXISTS_KEYSPECS,
    .args = (RedisModuleCommandArg *)CF_MEXISTS_ARGS,
};

// ===============================
// CF.RESERVE key capacity [BUCKETSIZE bucketsize] [MAXITERATIONS maxiterations] [EXPANSION expansion]
// ===============================
static const RedisModuleCommandKeySpec CF_RESERVE_KEYSPECS[] = {
    {.notes = "",
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
// CF.SCANDUMP key iterator
// ===============================
static const RedisModuleCommandKeySpec CF_SCANDUMP_KEYSPECS[] = {
    {.notes = "",
     .flags = REDISMODULE_CMD_KEY_RW,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
    {0}};

static const RedisModuleCommandArg CF_SCANDUMP_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
    {.name = "iterator", .type = REDISMODULE_ARG_TYPE_INTEGER},
    {0}};

static const RedisModuleCommandInfo CF_SCANDUMP_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary = "Begins an incremental save of the bloom filter",
    .complexity = "O(n), where n is the capacity",
    .since = "1.0.0",
    .arity = 3,
    .key_specs = (RedisModuleCommandKeySpec *)CF_SCANDUMP_KEYSPECS,
    .args = (RedisModuleCommandArg *)CF_SCANDUMP_ARGS,
};

int RegisterCFCommandInfos(RedisModuleCtx *ctx) {
    RedisModuleCommand *add_cmd = RedisModule_GetCommand(ctx, "cf.add");
    if (!add_cmd)
        return REDISMODULE_ERR;
    if (RedisModule_SetCommandInfo(add_cmd, &CF_ADD_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModuleCommand *addnx_cmd = RedisModule_GetCommand(ctx, "cf.addnx");
    if (!addnx_cmd)
        return REDISMODULE_ERR;
    if (RedisModule_SetCommandInfo(addnx_cmd, &CF_ADDNX_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModuleCommand *count_cmd = RedisModule_GetCommand(ctx, "cf.count");
    if (!count_cmd)
        return REDISMODULE_ERR;
    if (RedisModule_SetCommandInfo(count_cmd, &CF_COUNT_INFO) == REDISMODULE_ERR) {
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

    RedisModuleCommand *mexists_cmd = RedisModule_GetCommand(ctx, "cf.mexists");
    if (!mexists_cmd)
        return REDISMODULE_ERR;
    if (RedisModule_SetCommandInfo(mexists_cmd, &CF_MEXISTS_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModuleCommand *reserve_cmd = RedisModule_GetCommand(ctx, "cf.reserve");
    if (!reserve_cmd)
        return REDISMODULE_ERR;
    if (RedisModule_SetCommandInfo(reserve_cmd, &CF_RESERVE_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModuleCommand *scandump_cmd = RedisModule_GetCommand(ctx, "cf.scandump");
    if (!scandump_cmd)
        return REDISMODULE_ERR;
    if (RedisModule_SetCommandInfo(scandump_cmd, &CF_SCANDUMP_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
