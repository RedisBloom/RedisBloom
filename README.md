# ReBloom - Bloom Filter Module for Redis

This module provides a scalable bloom filter as a Redis data type. Bloom filters
are probabilistic data structures that do a very good job of quickly
determining if something is contained within a set.

[![CircleCI](https://circleci.com/gh/RedisLabsModules/rebloom.svg?style=svg)](https://circleci.com/gh/RedisLabsModules/rebloom)


## Quick Start Guide
1. [Launch ReBloom with Docker](#launch-rebloom-with-docker)
1. [Use Rebloom with redis-cli](#use-rebloom-with-redis-cli)

Note: You can also [build and load the module](#building-and-loading-rebloom) yourself.

You can find a command reference in [Commands.md](docs/Commands.md)


### 1. Launch ReBloom with Docker
```
docker run -p 6379:6379 --name redis-rebloom redislabs/rebloom:latest
```

### 2. Use ReBloom with `redis-cli`
```
docker exec -it redis-rebloom bash

# redis-cli
# 127.0.0.1:6379> 
```

Start a new bloom filter by adding a new item
```
# 127.0.0.1:6379> BF.ADD newFilter foo
(integer) 1
``` 

 Checking if an item exists in the filter
```
# 127.0.0.1:6379> BF.EXISTS newFilter foo
(integer) 1
```


## Building and Loading ReBloom

In order to use this module, build it using `make` and load it into Redis.

### Loading

**Invoking redis with the module loaded**

```
$ redis-server --loadmodule /path/to/rebloom.so
```

You can find a command reference in [docs/Commands.md](docs/Commands.md)

