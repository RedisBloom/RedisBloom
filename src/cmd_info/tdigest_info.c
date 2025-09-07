#include "redismodule.h"
// ===============================
// TDIGEST.ADD
// ===============================
static const RedisModuleCommandKeySpec TDIGEST_ADD_KEYSPECS[] = {
    {.notes = "is the key name for an existing t-digest sketch.",
     .flags = REDISMODULE_CMD_KEY_RW,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
    {0}};

static const RedisModuleCommandArg TDIGEST_ADD_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
    {.name = "value", .type = REDISMODULE_ARG_TYPE_DOUBLE, .flags = REDISMODULE_CMD_ARG_MULTIPLE},
    {0}};

static const RedisModuleCommandInfo TDIGEST_ADD_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary = "Adds one or more observations to a t-digest sketch.",
    .complexity = "O(N), where N is the number of samples to add",
    .since = "2.4.0",
    .arity = -3,
    .key_specs = (RedisModuleCommandKeySpec *)TDIGEST_ADD_KEYSPECS,
    .args = (RedisModuleCommandArg *)TDIGEST_ADD_ARGS,
};

// ===============================
// TDIGEST.BYRANK
// ===============================
static const RedisModuleCommandKeySpec TDIGEST_BYRANK_KEYSPECS[] = {
    {.notes = "is the key name for an existing t-digest sketch.",
     .flags = REDISMODULE_CMD_KEY_RO,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
    {0}};

static const RedisModuleCommandArg TDIGEST_BYRANK_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
    {.name = "rank", .type = REDISMODULE_ARG_TYPE_DOUBLE, .flags = REDISMODULE_CMD_ARG_MULTIPLE},
    {0}};

static const RedisModuleCommandInfo TDIGEST_BYRANK_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary = "Returns the value at the given rank in the t-digest sketch.",
    .complexity = "O(N) where N is the number of ranks specified",
    .since = "2.4.0",
    .arity = -3,
    .key_specs = (RedisModuleCommandKeySpec *)TDIGEST_BYRANK_KEYSPECS,
    .args = (RedisModuleCommandArg *)TDIGEST_BYRANK_ARGS,
};

int RegisterTDigestCommandInfos(RedisModuleCtx *ctx) {
    RedisModuleCommand *cmd_add = RedisModule_GetCommand(ctx, "tdigest.add");
    if (!cmd_add) {
        return REDISMODULE_ERR;
    }
    if (RedisModule_SetCommandInfo(cmd_add, &TDIGEST_ADD_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModuleCommand *cmd_byrank = RedisModule_GetCommand(ctx, "tdigest.byrank");
    if (!cmd_byrank) {
        return REDISMODULE_ERR;
    }
    if (RedisModule_SetCommandInfo(cmd_byrank, &TDIGEST_BYRANK_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
