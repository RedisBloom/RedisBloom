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

## Default parameters

!!! warning "Note on using initialization default sizes"
    A filter should always be sized for the expected capacity and the desired error-rate.
    Using the `INSERT` family commands with the default values should be used in cases where many small filter exist and the expectation is most will remain at about that size.
    Not optimizing a filter for its intended use will result in degradation of performance and memory efficiency.

### Error rate and Initial Size for Bloom Filter
You can adjust the default error ratio and the initial filter size (for bloom filters)
using the `ERROR_RATE` and `INITIAL_SIZE` options respectively when loading the
module, e.g.

```
$ redis-server --loadmodule /path/to/redisbloom.so INITIAL_SIZE 400 ERROR_RATE 0.004
```

The default error rate is `0.01` and the default initial capacity is `100`.

### Initial Size for Cuckoo Filter

For Cuckoo filter, the default capacity is 1024.