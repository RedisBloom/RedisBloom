---
title: "Configuration Parameters"
linkTitle: "Configuration"
weight: 5
description: >
    RedisBloom supports multiple module configuration parameters. All of these parameters can only be set at load-time.
---

## Setting configuration parameters on module load

Setting configuration parameters at load-time is done by appending arguments after the `--loadmodule` argument when starting a server from the command line or after the `loadmodule` directive in a Redis config file. For example:

In [redis.conf](https://redis.io/docs/manual/config/):

```sh
loadmodule ./redisbloom.so [OPT VAL]...
```

From The [Redis CLI](https://redis.io/docs/manual/cli/), using the [MODULE LOAD](https://redis.io/commands/module-load/) command:

```
127.0.0.6379> MODULE LOAD redisbloom.so [OPT VAL]...
```

From the command line:

```sh
$ redis-server --loadmodule ./redisbloom.so [OPT VAL]...
```

## RedisBloom configuration parameters

The following table summerizes which configuration parameters can be set at module load-time and which can be set on run-time:

| Configuration Parameter                 | Load-time          | Run-time             |
| :-------                                | :-----             | :-----------         |
| [ERROR_RATE](#error_rate)               | :white_check_mark: | :white_large_square: |
| [INITIAL_SIZE](#initial_size)           | :white_check_mark: | :white_large_square: |
| [CF_MAX_EXPANSIONS](#cf_max_expansions) | :white_check_mark: | :white_large_square: |


## Default parameters

!!! warning "Note on using initialization default sizes"
    A filter should always be sized for the expected capacity and the desired error-rate.
    Using the `INSERT` family commands with the default values should be used in cases where many small filter exist and the expectation is most will remain at about that size.
    Not optimizing a filter for its intended use will result in degradation of performance and memory efficiency.

### ERROR_RATE

Default error ratio for Bloom filters.

#### Default

`0.01`

#### Example

```
$ redis-server --loadmodule /path/to/redisbloom.so ERROR_RATE 0.004
```

### INITIAL_SIZE

Default initial capacity for Bloom filters.

#### Default

`100`

#### Example

```
$ redis-server --loadmodule /path/to/redisbloom.so INITIAL_SIZE 400
```

### CF_MAX_EXPANSIONS

Default maximum expansions for Cuckoo filters.

#### Default

`32`

#### Example

```
$ redis-server --loadmodule /path/to/redisbloom.so CF_MAX_EXPANSIONS 16
```
