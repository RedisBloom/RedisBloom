# xxHash

`xxhash.h` and `LICENSE` are vendored from xxHash v0.8.3:
https://github.com/Cyan4973/xxHash/tree/v0.8.3

RedisBloom uses `XXH_INLINE_ALL` in `hash_config.c`. Dependency updates must not
change the semantics of an existing hash mapping version.
