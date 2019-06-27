[![GitHub issues](https://img.shields.io/github/release/RedisLabsModules/redisbloom.svg)](https://github.com/RedisBloom/RedisBloom/releases/latest)
[![CircleCI](https://circleci.com/gh/RedisBloom/RedisBloom.svg?style=svg)](https://circleci.com/gh/RedisBloom/RedisBloom)
[![Docker Cloud Build Status](https://img.shields.io/docker/cloud/build/redislabs/rebloom.svg)](https://hub.docker.com/r/redislabs/rebloom/builds/)
[![codecov](https://codecov.io/gh/RedisBloom/RedisBloom/branch/master/graph/badge.svg)](https://codecov.io/gh/RedisBloom/RedisBloom)

# RedisBloom - Bloom Filter Module for Redis

RedisBloom module provides four datatypes, a Scalable **Bloom Filter** and **Cuckoo Filter**, a **Count-Mins-Sketch** and a **Top-K**.
**Bloom and Cuckoo filters** are used to determine (with a given degree of certainty) whether an item is present or absent from a collection. While **Count-Min Sketch** is used to approximate count of items in sub-linear space and **Top-K** maintains a list of K most frequent items.

## Quick Start Guide
1. [Launch RedisBloom with Docker](#launch-redisbloom-with-docker)
1. [Use RedisBloom with redis-cli](#use-redisbloom-with-redis-cli)

Note: You can also [build and load the module](#building-and-loading-redisbloom) yourself.

### 1. Launch RedisBloom with Docker
```
docker run -p 6379:6379 --name redis-redisbloom redislabs/rebloom:latest
```

### 2. Use RedisBloom with `redis-cli`
```
docker exec -it redis-redisbloom bash

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

## Building and Loading RedisBloom
In order to use this module, build it using `make` and load it into Redis.

### Loading

**Invoking redis with the module loaded**
```
$ redis-server --loadmodule /path/to/redisbloom.so
```

## Client libraries
| Project | Language | License | Author | URL |
| ------- | -------- | ------- | ------ | --- |
| redisbloom-py | Python | BSD | [Redis Labs](https://redislabs.com) | [GitHub](https://github.com/RedisBloom/redisbloom-py) |
| JReBloom | Java | BSD | [Redis Labs](https://redislabs.com) | [GitHub](https://github.com/RedisBloom/JReBloom) |
| rebloom | JavaScript | MIT | [Albert Team](https://cvitae.now.sh/) | [GitHub](https://github.com/albert-team/rebloom) |

## Documentation
Documentation and full command reference at [redisbloom.io](http://redisbloom.io).

## License
Redis Source Available License Agreement - see [LICENSE](LICENSE)
