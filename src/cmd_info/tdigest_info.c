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
    .summary = "Adds one or more observations to a t-digest sketch",
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
    .summary = "Returns, for each input rank, an estimation of the value (floating-point) with that rank",
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
    .summary = "Returns, for each input reverse rank, an estimation of the value (floating-point) "
               "with that reverse rank",
    .complexity = "O(1)",
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
    .summary = "Allocates memory and initializes a new t-digest sketch",
    .complexity = "O(1)",
    .since = "2.4.0",
    .arity = -2,
    .key_specs = (RedisModuleCommandKeySpec *)TDIGEST_CREATE_KEYSPECS,
    .args = (RedisModuleCommandArg *)TDIGEST_CREATE_ARGS,
};

// ===============================
// TDIGEST.INFO key
// ===============================
static const RedisModuleCommandKeySpec TDIGEST_INFO_KEYSPECS[] = {
    {.notes = "",
     .flags = REDISMODULE_CMD_KEY_RO,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
    {0}};

static const RedisModuleCommandArg TDIGEST_INFO_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0}, {0}};

static const RedisModuleCommandInfo TDIGEST_INFO_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary = "Returns information and statistics about a t-digest sketch",
    .complexity = "O(1)",
    .since = "2.4.0",
    .arity = 2,
    .key_specs = (RedisModuleCommandKeySpec *)TDIGEST_INFO_KEYSPECS,
    .args = (RedisModuleCommandArg *)TDIGEST_INFO_ARGS,
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
    .summary = "Returns the maximum observation value from a t-digest sketch",
    .complexity = "O(1)",
    .since = "2.4.0",
    .arity = 2,
    .key_specs = (RedisModuleCommandKeySpec *)TDIGEST_MAX_KEYSPECS,
    .args = (RedisModuleCommandArg *)TDIGEST_MAX_ARGS,
};

// ===============================
// TDIGEST.MERGE destination-key numkeys source-key [source-key ...] [COMPRESSION compression] [OVERRIDE]
// ===============================
static const RedisModuleCommandKeySpec TDIGEST_MERGE_KEYSPECS[] = {
    {.notes = "",
     .flags = REDISMODULE_CMD_KEY_RW,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
    {0}};

static const RedisModuleCommandArg TDIGEST_MERGE_ARGS[] = {
    {.name = "destination-key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
    {.name = "numkeys", .type = REDISMODULE_ARG_TYPE_INTEGER},
    {.name = "source-key",
     .type = REDISMODULE_ARG_TYPE_KEY,
     .key_spec_index = 1,
     .flags = REDISMODULE_CMD_ARG_MULTIPLE},
    {.name = "compression",
     .type = REDISMODULE_ARG_TYPE_BLOCK,
     .flags = REDISMODULE_CMD_ARG_OPTIONAL,
     .token = "COMPRESSION",
     .subargs =
         (RedisModuleCommandArg[]){{.name = "compression", .type = REDISMODULE_ARG_TYPE_INTEGER},
                                   {0}}},
    {.name = "override",
     .type = REDISMODULE_ARG_TYPE_PURE_TOKEN,
     .flags = REDISMODULE_CMD_ARG_OPTIONAL,
     .token = "OVERRIDE"},
    {0}};

static const RedisModuleCommandInfo TDIGEST_MERGE_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary = "Merges multiple t-digest sketches into a single sketch",
    .complexity =
        "O(N*K), where N is the number of centroids and K being the number of input sketches",
    .since = "2.4.0",
    .arity = -4,
    .key_specs = (RedisModuleCommandKeySpec *)TDIGEST_MERGE_KEYSPECS,
    .args = (RedisModuleCommandArg *)TDIGEST_MERGE_ARGS,
};

// ===============================
// TDIGEST.MIN key
// ===============================
static const RedisModuleCommandKeySpec TDIGEST_MIN_KEYSPECS[] = {
    {.notes = "",
     .flags = REDISMODULE_CMD_KEY_RO,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
    {0}};

static const RedisModuleCommandArg TDIGEST_MIN_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0}, {0}};

static const RedisModuleCommandInfo TDIGEST_MIN_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary = "Returns the minimum observation value from a t-digest sketch",
    .complexity = "O(1)",
    .since = "2.4.0",
    .arity = 2,
    .key_specs = (RedisModuleCommandKeySpec *)TDIGEST_MIN_KEYSPECS,
    .args = (RedisModuleCommandArg *)TDIGEST_MIN_ARGS,
};

// ===============================
// TDIGEST.QUANTILE key quantile [quantile ...]
// ===============================
static const RedisModuleCommandKeySpec TDIGEST_QUANTILE_KEYSPECS[] = {
    {.notes = "",
     .flags = REDISMODULE_CMD_KEY_RO,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
    {0}};

static const RedisModuleCommandArg TDIGEST_QUANTILE_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
    {.name = "quantile",
     .type = REDISMODULE_ARG_TYPE_DOUBLE,
     .flags = REDISMODULE_CMD_ARG_MULTIPLE},
    {0}};

static const RedisModuleCommandInfo TDIGEST_QUANTILE_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary = "Returns, for each input fraction, an estimation of the value (floating point) that "
               "is smaller than the given fraction of observations",
    .complexity = "O(1)",
    .since = "2.4.0",
    .arity = -3,
    .key_specs = (RedisModuleCommandKeySpec *)TDIGEST_QUANTILE_KEYSPECS,
    .args = (RedisModuleCommandArg *)TDIGEST_QUANTILE_ARGS,
};

// ===============================
// TDIGEST.RANK key value [value ...]
// ===============================
static const RedisModuleCommandKeySpec TDIGEST_RANK_KEYSPECS[] = {
    {.notes = "",
     .flags = REDISMODULE_CMD_KEY_RO,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
    {0}};

static const RedisModuleCommandArg TDIGEST_RANK_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
    {.name = "value", .type = REDISMODULE_ARG_TYPE_DOUBLE, .flags = REDISMODULE_CMD_ARG_MULTIPLE},
    {0}};

static const RedisModuleCommandInfo TDIGEST_RANK_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary = "Returns, for each input value (floating-point), the estimated rank of the value "
               "(the number of observations in the sketch that are smaller than the value + half "
               "the number of observations that are equal to the value)",
    .complexity = "O(1)",
    .since = "2.4.0",
    .arity = -3,
    .key_specs = (RedisModuleCommandKeySpec *)TDIGEST_RANK_KEYSPECS,
    .args = (RedisModuleCommandArg *)TDIGEST_RANK_ARGS,
};

// ===============================
// TDIGEST.RESET key
// ===============================
static const RedisModuleCommandKeySpec TDIGEST_RESET_KEYSPECS[] = {
    {.notes = "",
     .flags = REDISMODULE_CMD_KEY_RW,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
    {0}};

static const RedisModuleCommandArg TDIGEST_RESET_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0}, {0}};

static const RedisModuleCommandInfo TDIGEST_RESET_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary = "Resets a t-digest sketch: empty the sketch and re-initializes it.",
    .complexity = "O(1)",
    .since = "2.4.0",
    .arity = 2,
    .key_specs = (RedisModuleCommandKeySpec *)TDIGEST_RESET_KEYSPECS,
    .args = (RedisModuleCommandArg *)TDIGEST_RESET_ARGS,
};

// ===============================
// TDIGEST.REVRANK key value [value ...]
// ===============================
static const RedisModuleCommandKeySpec TDIGEST_REVRANK_KEYSPECS[] = {
    {.notes = "",
     .flags = REDISMODULE_CMD_KEY_RO,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
    {0}};

static const RedisModuleCommandArg TDIGEST_REVRANK_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
    {.name = "value", .type = REDISMODULE_ARG_TYPE_DOUBLE, .flags = REDISMODULE_CMD_ARG_MULTIPLE},
    {0}};

static const RedisModuleCommandInfo TDIGEST_REVRANK_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary = "Returns, for each input value (floating-point), the estimated reverse rank of the "
               "value (the number of observations in the sketch that are larger than the value + "
               "half the number of observations that are equal to the value)",
    .complexity = "O(1)",
    .since = "2.4.0",
    .arity = -3,
    .key_specs = (RedisModuleCommandKeySpec *)TDIGEST_REVRANK_KEYSPECS,
    .args = (RedisModuleCommandArg *)TDIGEST_REVRANK_ARGS,
};

// ===============================
// TDIGEST.TRIMMED_MEAN key low_cut_quantile high_cut_quantile
// ===============================
static const RedisModuleCommandKeySpec TDIGEST_TRIMMED_MEAN_KEYSPECS[] = {
    {.notes = "",
     .flags = REDISMODULE_CMD_KEY_RO,
     .begin_search_type = REDISMODULE_KSPEC_BS_INDEX,
     .bs.index = {.pos = 1},
     .find_keys_type = REDISMODULE_KSPEC_FK_RANGE,
     .fk.range = {.lastkey = 0, .keystep = 1, .limit = 0}},
    {0}};

static const RedisModuleCommandArg TDIGEST_TRIMMED_MEAN_ARGS[] = {
    {.name = "key", .type = REDISMODULE_ARG_TYPE_KEY, .key_spec_index = 0},
    {.name = "low_cut_quantile", .type = REDISMODULE_ARG_TYPE_DOUBLE},
    {.name = "high_cut_quantile", .type = REDISMODULE_ARG_TYPE_DOUBLE},
    {0}};

static const RedisModuleCommandInfo TDIGEST_TRIMMED_MEAN_INFO = {
    .version = REDISMODULE_COMMAND_INFO_VERSION,
    .summary = "Returns an estimation of the mean value from the sketch, excluding observation "
               "values outside the low and high cutoff quantiles",
    .complexity = "O(N) where N is the number of centroids",
    .since = "2.4.0",
    .arity = 4,
    .key_specs = (RedisModuleCommandKeySpec *)TDIGEST_TRIMMED_MEAN_KEYSPECS,
    .args = (RedisModuleCommandArg *)TDIGEST_TRIMMED_MEAN_ARGS,
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

    RedisModuleCommand *cmd_info = RedisModule_GetCommand(ctx, "tdigest.info");
    if (!cmd_info) {
        return REDISMODULE_ERR;
    }
    if (RedisModule_SetCommandInfo(cmd_info, &TDIGEST_INFO_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModuleCommand *cmd_max = RedisModule_GetCommand(ctx, "tdigest.max");
    if (!cmd_max) {
        return REDISMODULE_ERR;
    }
    if (RedisModule_SetCommandInfo(cmd_max, &TDIGEST_MAX_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModuleCommand *cmd_merge = RedisModule_GetCommand(ctx, "tdigest.merge");
    if (!cmd_merge) {
        return REDISMODULE_ERR;
    }
    if (RedisModule_SetCommandInfo(cmd_merge, &TDIGEST_MERGE_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModuleCommand *cmd_min = RedisModule_GetCommand(ctx, "tdigest.min");
    if (!cmd_min) {
        return REDISMODULE_ERR;
    }
    if (RedisModule_SetCommandInfo(cmd_min, &TDIGEST_MIN_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModuleCommand *cmd_quantile = RedisModule_GetCommand(ctx, "tdigest.quantile");
    if (!cmd_quantile) {
        return REDISMODULE_ERR;
    }
    if (RedisModule_SetCommandInfo(cmd_quantile, &TDIGEST_QUANTILE_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModuleCommand *cmd_rank = RedisModule_GetCommand(ctx, "tdigest.rank");
    if (!cmd_rank) {
        return REDISMODULE_ERR;
    }
    if (RedisModule_SetCommandInfo(cmd_rank, &TDIGEST_RANK_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModuleCommand *cmd_reset = RedisModule_GetCommand(ctx, "tdigest.reset");
    if (!cmd_reset) {
        return REDISMODULE_ERR;
    }
    if (RedisModule_SetCommandInfo(cmd_reset, &TDIGEST_RESET_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModuleCommand *cmd_revrank = RedisModule_GetCommand(ctx, "tdigest.revrank");
    if (!cmd_revrank) {
        return REDISMODULE_ERR;
    }
    if (RedisModule_SetCommandInfo(cmd_revrank, &TDIGEST_REVRANK_INFO) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModuleCommand *cmd_trimmed_mean = RedisModule_GetCommand(ctx, "tdigest.trimmed_mean");
    if (!cmd_trimmed_mean) {
        return REDISMODULE_ERR;
    }
    if (RedisModule_SetCommandInfo(cmd_trimmed_mean, &TDIGEST_TRIMMED_MEAN_INFO) ==
        REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
