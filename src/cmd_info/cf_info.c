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

    return REDISMODULE_OK;
}
