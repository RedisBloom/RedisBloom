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
    return REDISMODULE_OK;
}
