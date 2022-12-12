use redis_module::native_types::RedisType;
use redis_module::{Context, RedisError, RedisModuleTypeMethods, RedisResult, RedisString, NextArg, REDIS_OK};
use std::os::raw::c_void;

use growable_bloom_filter::GrowableBloom;

const BLOOM_FILTER_TYPE_NAME: &str = "MBbloom--";
const BLOOM_FILTER_TYPE_VERSION: i32 = 1;
pub static BLOOM_FILTER_TYPE: RedisType = RedisType::new(
    BLOOM_FILTER_TYPE_NAME,
    BLOOM_FILTER_TYPE_VERSION,
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

unsafe extern "C" fn free(_value: *mut c_void) {
    // Box::from_raw(value.cast::<MyType>());
}

// BF.RESERVE key error_rate capacity [EXPANSION expansion] [NONSCALING]
pub fn reserve(ctx: &Context, args: Vec<RedisString>) -> RedisResult {
    if args.len() < 4 {
        return Err(RedisError::WrongArity);
    }

    let mut args = args.into_iter().skip(1);
    let key = args.next_arg()?;
    let desired_error_prob = args.next_f64()?;
    let est_insertions = args.next_u64()?;

    let key = ctx.open_key_writable(&key);

    if let Some(_value) = key.get_value::<GrowableBloom>(&BLOOM_FILTER_TYPE)? {
        Err(RedisError::Str("ERR item exists"))
    } else {
        let bloom = GrowableBloom::new(desired_error_prob, est_insertions as usize);
        key.set_value(&BLOOM_FILTER_TYPE, bloom)?;
        REDIS_OK
    }
}

// BF.ADD key item
pub fn add(ctx: &Context, args: Vec<RedisString>) -> RedisResult {
    if args.len() < 3 {
        return Err(RedisError::WrongArity);
    }

    let mut args = args.into_iter().skip(1);
    let key = args.next_arg()?;
    let item = args.next_arg()?;
    
    let key = ctx.open_key_writable(&key);

    if let Some(bloom) = key.get_value::<GrowableBloom>(&BLOOM_FILTER_TYPE)? {
        bloom.insert(item.as_slice());
    } else {
        let mut bloom = GrowableBloom::new(0.1, 1000);
        bloom.insert(item.as_slice());
        key.set_value(&BLOOM_FILTER_TYPE, bloom)?;
    }
    REDIS_OK
}


// BF.EXISTS key item
pub fn exits(ctx: &Context, args: Vec<RedisString>) -> RedisResult {
    if args.len() < 3 {
        return Err(RedisError::WrongArity);
    }

    let mut args = args.into_iter().skip(1);
    let key = args.next_arg()?;
    let item = args.next_arg()?;

    let key = ctx.open_key(&key);

    if let Some(bloom) = key.get_value::<GrowableBloom>(&BLOOM_FILTER_TYPE)? {
        Ok( (bloom.contains(item.as_slice()) as i64).into())
    } else {
        Ok(0i64.into())
    }
}