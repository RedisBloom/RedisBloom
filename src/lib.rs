/*
 * Copyright Redis Ltd. 2016 - present
 * Licensed under your choice of the Redis Source Available License 2.0 (RSALv2) or
 * the Server Side Public License v1 (SSPLv1).
 */

#[macro_use]
extern crate redis_module;
mod bloom_filter;

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
        ["BF.RESERVE", bloom_filter::reserve, "", 1, 1, 1],
        ["BF.ADD", bloom_filter::add, "", 1, 1, 1],
        ["BF.EXISTS", bloom_filter::exits, "", 1, 1, 1],
    ],
}
