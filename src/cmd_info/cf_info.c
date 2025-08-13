#include "redismodule.h"

/* Static const metadata for cf.reserve */
static const RedisModuleCommandKeySpec CF_RESERVE_KEYSPECS[] = {
    {.notes = "Key must be a non-existing key",
     .flags = REDISMODULE_CMD_KEY_RW,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 1, .keystep = 0, .limit = 0}},
    {0}};

static const RedisModuleCommandArg CF_RESERVE_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
    {.name = "capacity", .type = REDISMODULE_ARG_TYPE_INTEGER},
    {.name = "bucketsize",
     .type = REDISMODULE_ARG_TYPE_INTEGER,
     .flags = REDISMODULE_CMD_ARG_OPTIONAL},
    {.name = "maxiterations",
     .type = REDISMODULE_ARG_TYPE_INTEGER,
     .flags = REDISMODULE_CMD_ARG_OPTIONAL},
    {.name = "expansion",
     .type = REDISMODULE_ARG_TYPE_INTEGER,
     .flags = REDISMODULE_CMD_ARG_OPTIONAL},
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

int RegisterCFCommandInfos(RedisModuleCtx *ctx) {
    RedisModuleCommand *cmd = RedisModule_GetCommand(ctx, "cf.reserve");
    if (!cmd)
        return REDISMODULE_ERR;
    if (RedisModule_SetCommandInfo(cmd, &CF_RESERVE_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }
    return REDISMODULE_OK;
}
