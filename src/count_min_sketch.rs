use redis_module::native_types::RedisType;
use redis_module::{
    Context, NextArg, RedisError, RedisModuleTypeMethods, RedisResult, RedisString, RedisValue,
    REDIS_OK,
};
use std::os::raw::c_void;

use pdatastructs::countminsketch::CountMinSketch;

const TYPE_NAME: &str = "CMSk-TYPE";
const TYPE_VERSION: i32 = 1;

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
    drop(Box::from_raw(value.cast::<CountMinSketch<&[u8], u64>>()));
}

// CMS.INITBYDIM key width depth
pub fn init_by_dim(ctx: &Context, args: Vec<RedisString>) -> RedisResult {
    if args.len() < 4 {
        return Err(RedisError::WrongArity);
    }

    let mut args = args.into_iter().skip(1);
    let key = args.next_arg()?;
    let width = args.next_u64()?;
    let depth = args.next_u64()?;

    let key = ctx.open_key_writable(&key);

    if let Some(_value) = key.get_value::<CountMinSketch<&[u8], u64>>(&COUNT_MIN_SKETCH_TYPE)? {
        Err(RedisError::Str("CMS: key already exists"))
    } else {
        let sketch: CountMinSketch<&[u8], u64> = CountMinSketch::with_params(width as usize, depth as usize);
        key.set_value(&COUNT_MIN_SKETCH_TYPE, sketch)?;
        REDIS_OK
    }
}

// CMS.INITBYPROB key error probability
pub fn init_by_prob(ctx: &Context, args: Vec<RedisString>) -> RedisResult {
    if args.len() < 4 {
        return Err(RedisError::WrongArity);
    }

    let mut args = args.into_iter().skip(1);
    let key = args.next_arg()?;
    let error = args.next_f64()?;
    let probability = args.next_f64()?;

    // TODO error on negative values
    // - `epsilon > 0`
    // - `delta > 0` and `delta < 1`

    let key = ctx.open_key_writable(&key);

    if let Some(_value) = key.get_value::<CountMinSketch<&[u8], u64>>(&COUNT_MIN_SKETCH_TYPE)? {
        Err(RedisError::Str("CMS: key already exists"))
    } else {
        let sketch: CountMinSketch<&[u8], u64> = CountMinSketch::with_point_query_properties(probability, error);
        key.set_value(&COUNT_MIN_SKETCH_TYPE, sketch)?;
        REDIS_OK
    }
}

// // CMS.INCRBY key item increment [item increment ...]
// pub fn incr_by(ctx: &Context, args: Vec<RedisString>) -> RedisResult {

//     let length = args.len()%2;
//     if args.len() < 4 && length != 0 {
//         return Err(RedisError::WrongArity);
//     }

//     let mut args = args.into_iter().skip(1);
//     let key = args.next_arg()?;
    
//     let items: Vec<(RedisString, u64)> = vec![length/2 - 1];
//     while let Ok(arg) = args.next_arg() {
//         items.push((arg, args.next_u64())?);
//     }


//     let key = ctx.open_key_writable(&key);



//     let res = if let Some(sketch) = key.get_value::<CountMinSketch<&[u8], u64>>(&COUNT_MIN_SKETCH_TYPE)? {
//         sketch.add_n(item.as_slice(), &10);
//     } else {
//         let mut sketch = CountMinSketch::new(0.1, 1000);
//         let res = sketch.insert(item.as_slice());
//         key.set_value(&COUNT_MIN_SKETCH_TYPE, sketch)?;
//         res
//     };
//     Ok((res as i64).into())
// }


