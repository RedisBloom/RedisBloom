# ReBloom - Bloom Filter Module for Redis

This module provides a scalable bloom filter as a Redis data type. Bloom filters
are probabilistic data structures that do a very good job of quickly
determining if something is contained within a set.

[![Build Status](https://travis-ci.org/RedisLabsModules/rebloom.svg?branch=master)](https://travis-ci.org/RedisLabsModules/rebloom)

## Building

Use the `make` command to build and load it into Redis.

### Using

**Invoking redis with the module loaded**

```
$ redis-server --loadmodule /path/to/rebloom.so
```

**Adding items to filter**
```
127.0.0.1:6379> BF.MADD bf foo bar baz
1) (integer) 1
2) (integer) 1
3) (integer) 1
```

**Checking if items exist**
```
127.0.0.1:6379> BF.MEXISTS bf foo nonexist 3
1) (integer) 1
2) (integer) 0
3) (integer) 0
```

You can find a command reference in [docs/Commands.md](docs/Commands.md)


### Module Options

You can adjust the default error ratio and the initial filter size using
the `ERROR_RATE` and `INITIAL_SIZE` options respectively when loading the
module, e.g.:

```
$ redis-server --loadmodule /path/to/rebloom.so INITIAL_SIZE 400 ERROR_RATE 0.004
```

The default error rate is `0.01` and the default initial capacity is `100`.
