#include "redismodule.h"

// ===============================
// TDIGEST.ADD key value [value ...]
// ===============================
static const RedisModuleCommandKeySpec TDIGEST_ADD_KEYSPECS[] = {
    {.notes = "",
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
// TDIGEST.BYRANK key rank [rank ...]
// ===============================
static const RedisModuleCommandKeySpec TDIGEST_BYRANK_KEYSPECS[] = {
    {.notes = "",
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

// ===============================
// TDIGEST.BYREVRANK key reverse_rank [reverse_rank ...]
// ===============================
static const RedisModuleCommandKeySpec TDIGEST_BYREVRANK_KEYSPECS[] = {
    {.notes = "",
     .flags = REDISMODULE_CMD_KEY_RO,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
    {0}};

static const RedisModuleCommandArg TDIGEST_BYREVRANK_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
    {.name = "reverse_rank",
     .type = REDISMODULE_ARG_TYPE_DOUBLE,
     .flags = REDISMODULE_CMD_ARG_MULTIPLE},
    {0}};

static const RedisModuleCommandInfo TDIGEST_BYREVRANK_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary =
        "Returns, for each input reverse rank (revrank), an estimation of the floating-point value "
        "with that reverse rank. Multiple estimations can be retrieved in a single call.",
    .complexity = "O(N) where N is the number of reverse ranks specified.",
    .since = "2.4.0",
    .arity = -3,
    .key_specs = (RedisModuleCommandKeySpec *)TDIGEST_BYREVRANK_KEYSPECS,
    .args = (RedisModuleCommandArg *)TDIGEST_BYREVRANK_ARGS,
};

// ===============================
// TDIGEST.CDF key value [value ...]
// ===============================
static const RedisModuleCommandKeySpec TDIGEST_CDF_KEYSPECS[] = {
    {.notes = "",
     .flags = REDISMODULE_CMD_KEY_RO,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
    {0}};

static const RedisModuleCommandArg TDIGEST_CDF_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
    {.name = "value", .type = REDISMODULE_ARG_TYPE_DOUBLE, .flags = REDISMODULE_CMD_ARG_MULTIPLE},
    {0}};

static const RedisModuleCommandInfo TDIGEST_CDF_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary = "Returns, for each input value, an estimation of the floating-point fraction of "
               "(observations smaller than the given value + half the observations equal to the "
               "given value). Multiple fractions can be retrieved in a single call.",
    .complexity = "O(N) where N is the number of values specified.",
    .since = "2.4.0",
    .arity = -3,
    .key_specs = (RedisModuleCommandKeySpec *)TDIGEST_CDF_KEYSPECS,
    .args = (RedisModuleCommandArg *)TDIGEST_CDF_ARGS,
};

// ===============================
// TDIGEST.CREATE key [COMPRESSION compression]
// ===============================
static const RedisModuleCommandKeySpec TDIGEST_CREATE_KEYSPECS[] = {
    {.notes = "",
     .flags = REDISMODULE_CMD_KEY_RW,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
    {0}};

static const RedisModuleCommandArg TDIGEST_CREATE_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
    {.name = "compression_block",
     .type = REDISMODULE_ARG_TYPE_BLOCK,
     .flags = REDISMODULE_CMD_ARG_OPTIONAL,
     .subargs =
         (RedisModuleCommandArg[]){
             {.name = "compression_token",
              .type = REDISMODULE_ARG_TYPE_PURE_TOKEN,
              .token = "COMPRESSION"},
             {.name = "compression", .type = REDISMODULE_ARG_TYPE_INTEGER},
             {0},
         }},
    {0}};

static const RedisModuleCommandInfo TDIGEST_CREATE_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary = "Allocates memory and initializes a new t-digest sketch.",
    .complexity = "O(1)",
    .since = "2.4.0",
    .arity = -2,
    .key_specs = (RedisModuleCommandKeySpec *)TDIGEST_CREATE_KEYSPECS,
    .args = (RedisModuleCommandArg *)TDIGEST_CREATE_ARGS,
};

// ===============================
// TDIGEST.MAX key
// ===============================
static const RedisModuleCommandKeySpec TDIGEST_MAX_KEYSPECS[] = {
    {.notes = "",
     .flags = REDISMODULE_CMD_KEY_RO,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
    {0}};

static const RedisModuleCommandArg TDIGEST_MAX_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0}, {0}};

static const RedisModuleCommandInfo TDIGEST_MAX_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary = "Returns the maximum observation value from a t-digest sketch.",
    .complexity = "O(1)",
    .since = "2.4.0",
    .arity = 2,
    .key_specs = (RedisModuleCommandKeySpec *)TDIGEST_MAX_KEYSPECS,
    .args = (RedisModuleCommandArg *)TDIGEST_MAX_ARGS,
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

    RedisModuleCommand *cmd_byrevrank = RedisModule_GetCommand(ctx, "tdigest.byrevrank");
    if (!cmd_byrevrank) {
        return REDISMODULE_ERR;
    }
    if (RedisModule_SetCommandInfo(cmd_byrevrank, &TDIGEST_BYREVRANK_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModuleCommand *cmd_cdf = RedisModule_GetCommand(ctx, "tdigest.cdf");
    if (!cmd_cdf) {
        return REDISMODULE_ERR;
    }
    if (RedisModule_SetCommandInfo(cmd_cdf, &TDIGEST_CDF_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModuleCommand *cmd_create = RedisModule_GetCommand(ctx, "tdigest.create");
    if (!cmd_create) {
        return REDISMODULE_ERR;
    }
    if (RedisModule_SetCommandInfo(cmd_create, &TDIGEST_CREATE_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModuleCommand *cmd_max = RedisModule_GetCommand(ctx, "tdigest.max");
    if (!cmd_max) {
        return REDISMODULE_ERR;
    }
    if (RedisModule_SetCommandInfo(cmd_max, &TDIGEST_MAX_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
