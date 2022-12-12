use redis_module::native_types::RedisType;
use redis_module::{
    Context, NextArg, RedisError, RedisModuleTypeMethods, RedisResult, RedisString, RedisValue,
    REDIS_OK,
};
use std::os::raw::c_void;

use growable_bloom_filter::GrowableBloom;

const TYPE_NAME: &str = "CMSk-TYPE";
const TYPE_VERSION: i32 = 1;
const EXPANSION: &str = "EXPANSION";
const NONSCALING: &str = "NONSCALING";

pub static COUNT_MIN_SKETCH_TYPE: RedisType = RedisType::new(
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
    drop(Box::from_raw(value.cast::<GrowableBloom>()));
}

// BF.RESERVE key error_rate capacity [EXPANSION expansion] [NONSCALING]
pub fn reserve(ctx: &Context, args: Vec<RedisString>) -> RedisResult {
    if args.len() < 4 {
        return Err(RedisError::WrongArity);
    }

    let mut args = args.into_iter().skip(1);
    let key = args.next_arg()?;
    let error_rate = args.next_f64()?;
    let capacity = args.next_u64()?;
    let mut expansion = 1;
    let mut scaling = true;

    while let Ok(arg) = args.next_str() {
        match arg {
            arg if arg.eq_ignore_ascii_case(EXPANSION) => {
                expansion = args.next_u64().map_err(|_| RedisError::Str("ERR bad expansion"))?;
            }
            arg if arg.eq_ignore_ascii_case(NONSCALING) => {
                scaling = false;
            }
            _ => () // ignore unknown arguments for backward
        }
    }

    let key = ctx.open_key_writable(&key);

    if let Some(_value) = key.get_value::<GrowableBloom>(&COUNT_MIN_SKETCH_TYPE)? {
        Err(RedisError::Str("ERR item exists"))
    } else {
        let bloom = GrowableBloom::new(error_rate, capacity as usize);
        key.set_value(&COUNT_MIN_SKETCH_TYPE, bloom)?;
        REDIS_OK
    }
}

// CMS.INCRBY key item increment [item increment ...]
pub fn incr_by(ctx: &Context, args: Vec<RedisString>) -> RedisResult {
    if args.len() != 3 {
        return Err(RedisError::WrongArity);
    }

    let mut args = args.into_iter().skip(1);
    let key = args.next_arg()?;
    let item = args.next_arg()?;

    let key = ctx.open_key_writable(&key);

    let res = if let Some(bloom) = key.get_value::<GrowableBloom>(&COUNT_MIN_SKETCH_TYPE)? {
        bloom.insert(item.as_slice())
    } else {
        let mut bloom = GrowableBloom::new(0.1, 1000);
        let res = bloom.insert(item.as_slice());
        key.set_value(&COUNT_MIN_SKETCH_TYPE, bloom)?;
        res
    };
    Ok((res as i64).into())
}

// BF.MADD key item [item ...]
pub fn madd(ctx: &Context, args: Vec<RedisString>) -> RedisResult {
    if args.len() < 3 {
        return Err(RedisError::WrongArity);
    }

    let mut args = args.into_iter().skip(1);
    let key = args.next_arg()?;

    let key = ctx.open_key_writable(&key);

    let res = if let Some(bloom) = key.get_value::<GrowableBloom>(&COUNT_MIN_SKETCH_TYPE)? {
        args.map(|item| (bloom.insert(item.as_slice()) as i64).into())
            .collect()
    } else {
        let mut bloom = GrowableBloom::new(0.1, 1000);
        let res = args
            .map(|item| (bloom.insert(item.as_slice()) as i64).into())
            .collect();
        key.set_value(&COUNT_MIN_SKETCH_TYPE, bloom)?;
        res
    };
    Ok(RedisValue::Array(res))
}

// BF.EXISTS key item
pub fn exits(ctx: &Context, args: Vec<RedisString>) -> RedisResult {
    if args.len() != 3 {
        return Err(RedisError::WrongArity);
    }

    let mut args = args.into_iter().skip(1);
    let key = args.next_arg()?;
    let item = args.next_arg()?;

    let key = ctx.open_key(&key);

    if let Some(bloom) = key.get_value::<GrowableBloom>(&COUNT_MIN_SKETCH_TYPE)? {
        Ok((bloom.contains(item.as_slice()) as i64).into())
    } else {
        Ok(0i64.into())
    }
}
