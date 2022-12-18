use redis_module::native_types::RedisType;
use redis_module::{
    Context, NextArg, RedisError, RedisModuleTypeMethods, RedisResult, RedisString, RedisValue,
    REDIS_OK,
};
use std::os::raw::c_void;

use tdigest::TDigest;

const TYPE_NAME: &str = "TDIS-TYPE";
const TYPE_VERSION: i32 = 1;
const COMPRESSION: &str = "COMPRESSION";

struct Histogram {
    tdigest: TDigest,
}

pub static T_DIGEST_TYPE: RedisType = RedisType::new(
    TYPE_NAME,
    TYPE_VERSION,
    RedisModuleTypeMethods {
        version: redis_module::TYPE_METHOD_VERSION,

        rdb_load: None,    // Some(redisjson::type_methods::rdb_load),
        rdb_save: None,    // Some(redisjson::type_methods::rdb_save),
        aof_rewrite: None, // TODO add support
        free: Some(free),

        // Currently unused by Redis
        mem_usage: None, // Some(redisjson::type_methods::mem_usage),
        digest: None,

        // Auxiliary data (v2)
        aux_load: None,
        aux_save: None,
        aux_save_triggers: 0,

        free_effort: None,
        unlink: None,
        copy: None, // Some(redisjson::type_methods::copy),
        defrag: None,
    },
);

unsafe extern "C" fn free(value: *mut c_void) {
    drop(Box::from_raw(value.cast::<Histogram>()));
}

// TDIGEST.CREATE key [COMPRESSION compression]
pub fn create(ctx: &Context, args: Vec<RedisString>) -> RedisResult {
    if args.len() < 2 {
        return Err(RedisError::WrongArity);
    }

    let mut args = args.into_iter().skip(1);
    let key = args.next_arg()?;
    let mut compression = 1;

    while let Ok(arg) = args.next_str() {
        match arg {
            arg if arg.eq_ignore_ascii_case(COMPRESSION) => {
                // TODO handle "(error) ERR T-Digest: compression parameter needs to be a positive integer"
                compression = args.next_u64().map_err(|_| {
                    RedisError::Str("ERR T-Digest: error parsing compression parameter")
                })?;
            }
            _ => (), // ignore unknown arguments for backward
        }
    }

    let key = ctx.open_key_writable(&key);

    if let Some(_value) = key.get_value::<Histogram>(&T_DIGEST_TYPE)? {
        Err(RedisError::Str("ERR T-Digest: key already exists"))
    } else {
        let histogram = TDigest::new_with_size(compression as usize);
        key.set_value(&T_DIGEST_TYPE, histogram)?;
        REDIS_OK
    }
}

// TDIGEST.ADD key value [value ...]
pub fn add(ctx: &Context, args: Vec<RedisString>) -> RedisResult {
    if args.len() < 3 {
        return Err(RedisError::WrongArity);
    }

    let mut args = args.into_iter().skip(1);
    let key_string = args.next_arg()?;
    let key = ctx.open_key_writable(&key_string);

    if let Some(histogram) = key.get_value::<Histogram>(&T_DIGEST_TYPE)? {
        let values: Result<Vec<f64>, _> = args.map(|item| item.parse_float()).collect();
        values.map_or(
            Err(RedisError::Str("ERR T-Digest: error parsing val parameter")),
            |values| {
                histogram.tdigest = histogram.tdigest.merge_unsorted(values);
                REDIS_OK
            },
        )
    } else {
        Err(RedisError::Str("ERR T-Digest: key does not exist"))
    }
}

// TDIGEST.QUANTILE key quantile [quantile ...]
pub fn quantile(ctx: &Context, args: Vec<RedisString>) -> RedisResult {
    if args.len() < 3 {
        return Err(RedisError::WrongArity);
    }

    let mut args = args.into_iter().skip(1);
    let key = args.next_arg()?;
    let key = ctx.open_key(&key);

    if let Some(histogram) = key.get_value::<Histogram>(&T_DIGEST_TYPE)? {
        // TODO handle "(error) ERR T-Digest: quantile should be in [0,1]"

        let quantiles: Result<Vec<f64>, _> = args.map(|item| item.parse_float()).collect();
        quantiles.map_or(
            Err(RedisError::Str(
                "ERR T-Digest: error parsing quantile parameter",
            )),
            |quantiles| {
                let results: Vec<RedisValue> = quantiles
                    .iter()
                    .map(|quantile| histogram.tdigest.estimate_quantile(*quantile).into())
                    .collect();
                Ok(results.into())
            },
        )
    } else {
        Err(RedisError::Str("ERR T-Digest: key does not exist"))
    }
}
