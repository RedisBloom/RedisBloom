use cuckoofilter::CuckooFilter;
use redis_module::native_types::RedisType;
use redis_module::{
    Context, NextArg, RedisError, RedisModuleTypeMethods, RedisResult, RedisString, REDIS_OK,
};
use std::collections::hash_map::DefaultHasher;
use std::os::raw::c_void;

const TYPE_NAME: &str = "MBbloomCF";
const TYPE_VERSION: i32 = 1;
const EXPANSION: &str = "EXPANSION";
const BUCKETSIZE: &str = "BUCKETSIZE";
const MAXITERATIONS: &str = "MAXITERATIONS";

pub static CUCKOO_FILTER_TYPE: RedisType = RedisType::new(
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
    drop(Box::from_raw(value.cast::<CuckooFilter<&[u8]>>()));
}

// CF.RESERVE key capacity [BUCKETSIZE bucketsize]
//   [MAXITERATIONS maxiterations] [EXPANSION expansion]
pub fn reserve(ctx: &Context, args: Vec<RedisString>) -> RedisResult {
    if args.len() < 4 {
        return Err(RedisError::WrongArity);
    }

    let mut args = args.into_iter().skip(1);
    let key = args.next_arg()?;
    let mut expansion = 1;

    while let Ok(arg) = args.next_str() {
        match arg {
            arg if arg.eq_ignore_ascii_case(EXPANSION) => {
                expansion = args
                    .next_u64()
                    .map_err(|_| RedisError::Str("ERR bad expansion"))?;
            }
            _ => (), // ignore unknown arguments for backward
        }
    }

    let key = ctx.open_key_writable(&key);

    if let Some(_value) = key.get_value::<CuckooFilter<&[u8]>>(&CUCKOO_FILTER_TYPE)? {
        Err(RedisError::Str("ERR item exists"))
    } else {
        let filter = CuckooFilter::new();
        key.set_value(&CUCKOO_FILTER_TYPE, filter)?;
        REDIS_OK
    }
}

// CF.ADD key item
pub fn add(ctx: &Context, args: Vec<RedisString>) -> RedisResult {
    if args.len() != 3 {
        return Err(RedisError::WrongArity);
    }

    let mut args = args.into_iter().skip(1);
    let key = args.next_arg()?;
    let item = args.next_arg()?;

    let key = ctx.open_key_writable(&key);

    if let Some(filter) = key.get_value::<CuckooFilter<DefaultHasher>>(&CUCKOO_FILTER_TYPE)? {
        filter.add(item.as_slice())?
    } else {
        let mut filter = CuckooFilter::new();
        filter.add(item.as_slice())?;
        key.set_value(&CUCKOO_FILTER_TYPE, filter)?;
    };
    Ok(1i64.into())
}

// CF.EXISTS key item
pub fn exits(ctx: &Context, args: Vec<RedisString>) -> RedisResult {
    if args.len() != 3 {
        return Err(RedisError::WrongArity);
    }

    let mut args = args.into_iter().skip(1);
    let key = args.next_arg()?;
    let item = args.next_arg()?;

    let key = ctx.open_key(&key);

    if let Some(filter) = key.get_value::<CuckooFilter<DefaultHasher>>(&CUCKOO_FILTER_TYPE)? {
        Ok((filter.contains(item.as_slice()) as i64).into())
    } else {
        Ok(0i64.into())
    }
}
