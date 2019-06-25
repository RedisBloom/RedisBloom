# Run-time configuration

RedisBloom supports a few run-time configuration options that should be determined when loading the module.

## Passing Configuration Options During Loading

In general, passing configuration options is done by appending arguments after the `--loadmodule` argument in the command line, `loadmodule` configuration directive in a Redis config file, or the `MODULE LOAD` command. For example:

In redis.conf:

```
loadmodule redisearch.so OPT1 OPT2
```

From redis-cli:

```
127.0.0.6379> MODULE load redisearch.so OPT1 OPT2
```

From command line:

```
$ redis-server --loadmodule ./redisearch.so OPT1 OPT2
```

## Error rate and Initial Size
You can adjust the default error ratio and the initial filter size (for bloom filters)
using the `ERROR_RATE` and `INITIAL_SIZE` options respectively when loading the
module, e.g.

```
$ redis-server --loadmodule /path/to/redisbloom.so INITIAL_SIZE 400 ERROR_RATE 0.004
```

The default error rate is `0.01` and the default initial capacity is `100`.
