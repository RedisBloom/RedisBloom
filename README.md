[![GitHub issues](https://img.shields.io/github/release/RedisLabsModules/redisbloom.svg)](https://github.com/RedisBloom/RedisBloom/releases/latest)
[![CircleCI](https://circleci.com/gh/RedisBloom/RedisBloom.svg?style=svg)](https://circleci.com/gh/RedisBloom/RedisBloom)
[![DockerHub](https://dockerbuildbadges.quelltext.eu/status.svg?organization=rebloom&repository=rebloom)](https://hub.docker.com/r/redislabs/rebloom/builds/) 

# RedisBloom - Bloom Filter Module for Redis

This module provides three probabalistic data structures as Redis data types:
**Bloom Filters**, **Cuckoo Filters** and **Count-Min-Sketch**. The first two structures are similar in
their purpose but have different performance and functionality characteristics while the last is used to count the 
frequency of the different items in sub-linear space.

## Quick Start Guide
1. [Launch RedisBloom with Docker](#launch-redisbloom-with-docker)
1. [Use RedisBloom with redis-cli](#use-redisbloom-with-redis-cli)

Note: You can also [build and load the module](#building-and-loading-redisbloom) yourself.

You can find a command reference in [Bloom_Commands.md](docs/Bloom_Commands.md)


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
$ redis-server --loadmodule /path/to/rebloom.so
```

You can find a command reference in [docs/Bloom_Commands.md](docs/Bloom_Commands.md)

## Documentation

Read the docs at [redisbloom.io](http://redisbloom.io).


## License

Redis Source Available License Agreement - see [LICENSE](LICENSE)
