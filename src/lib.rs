/*
 * Copyright Redis Ltd. 2016 - present
 * Licensed under your choice of the Redis Source Available License 2.0 (RSALv2) or
 * the Server Side Public License v1 (SSPLv1).
 */

#[macro_use]
extern crate redis_module;
mod bloom_filter;
mod count_min_sketch;
mod cuckoo_filter;
mod t_digest;
mod top_k;

use bloom_filter::BLOOM_FILTER_TYPE;

pub const GIT_SHA: Option<&str> = std::option_env!("GIT_SHA");
pub const GIT_BRANCH: Option<&str> = std::option_env!("GIT_BRANCH");

pub const MODULE_NAME: &str = "bf";

/////////////////////////////////////////////////////

redis_module! {
    name: MODULE_NAME,
    version: 99_99_99,
    data_types: [BLOOM_FILTER_TYPE],
    commands: [
        ["BF.RESERVE", bloom_filter::reserve, "write deny-oom", 1, 1, 1],
        ["BF.ADD", bloom_filter::add, "write deny-oom", 1, 1, 1],
        ["BF.MADD", bloom_filter::madd, "write deny-oom", 1, 1, 1],
        ["BF.EXISTS", bloom_filter::exits, "readonly", 1, 1, 1],

        ["CF.RESERVE", cuckoo_filter::reserve, "write deny-oom", 1, 1, 1],
        ["CF.ADD", cuckoo_filter::add, "write deny-oom", 1, 1, 1],
        ["CF.EXISTS", cuckoo_filter::exits, "write deny-oom", 1, 1, 1],

        ["TDIGEST.CREATE", t_digest::create, "write deny-oom", 1, 1, 1],
        ["TDIGEST.ADD", t_digest::add, "write deny-oom", 1, 1, 1],
        ["TDIGEST.QUANTILE", t_digest::quantile, "write deny-oom", 1, 1, 1],

        ///////////
        // TODO just place holders
        ["TOPK.ADD", top_k::add, "write deny-oom", 1, 1, 1],
        ["CMS.INCRBY", count_min_sketch::incr_by, "write deny-oom", 1, 1, 1],
    ],
}
