[![GitHub issues](https://img.shields.io/github/release/RedisLabsModules/redisbloom.svg)](https://github.com/RedisBloom/RedisBloom/releases/latest)
[![CircleCI](https://circleci.com/gh/RedisBloom/RedisBloom.svg?style=svg)](https://circleci.com/gh/RedisBloom/RedisBloom)
[![Docker Cloud Build Status](https://img.shields.io/docker/cloud/build/redislabs/rebloom.svg)](https://hub.docker.com/r/redislabs/rebloom/builds/)
[![codecov](https://codecov.io/gh/RedisBloom/RedisBloom/branch/master/graph/badge.svg)](https://codecov.io/gh/RedisBloom/RedisBloom)
[![Forum](https://img.shields.io/badge/Forum-RedisBloom-blue)](https://forum.redislabs.com/c/modules/redisbloom)
[![Gitter](https://badges.gitter.im/RedisLabs/RedisBloom.svg)](https://gitter.im/RedisLabs/RedisBloom?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge)

# RedisBloom: A Redis Module for Probabilistic Data Structures

The RedisBloom module provides four probabilistic data types: a scalable **Bloom Filter**, a **Cuckoo Filter**, a **Count-Min-Sketch**, and a **Top-K**.
**Bloom and Cuckoo filters** are used to determine (with a given degree of certainty) whether an item is present in a collection. A **Count-Min Sketch** is used to approximately count items in sub-linear space. A **Top-K** maintains a list of the _K_ most frequent items.

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

Create a new bloom filter by adding a new item:
```
# 127.0.0.1:6379> BF.ADD newFilter foo
(integer) 1
``` 

 Check to see if an item exists in the filter:
```
# 127.0.0.1:6379> BF.EXISTS newFilter foo
(integer) 1
```

## Building and Loading RedisBloom
To use RedisBloom, build the module's shared library using `make`. Then pass the location of the shared library to the   `loadmodule` directive when starting Redis:

```
$ redis-server --loadmodule /path/to/redisbloom.so
```

## Client libraries
| Project | Language | License | Author | URL |
| ------- | -------- | ------- | ------ | --- |
| redisbloom-py | Python | BSD | [Redis Labs](https://redislabs.com) | [GitHub](https://github.com/RedisBloom/redisbloom-py) |
| JReBloom | Java | BSD | [Redis Labs](https://redislabs.com) | [GitHub](https://github.com/RedisBloom/JReBloom) |
| rebloom | JavaScript | MIT | [Albert Team](https://cvitae.now.sh/) | [GitHub](https://github.com/albert-team/rebloom) |
| phpredis-bloom | PHP | MIT | [Rafa Campoy](https://github.com/averias) | [GitHub](https://github.com/averias/phpredis-bloom) |
| phpRebloom | PHP | MIT | [Alessandro Balasco](https://github.com/palicao) | [GitHub](https://github.com/palicao/phpRebloom) |

## Documentation
Documentation and full command reference at [redisbloom.io](http://redisbloom.io).

## Mailing List / Forum
Got questions? Feel free to ask at the [RedisBloom mailing list](https://forum.redislabs.com/c/modules/redisbloom).

## License
Redis Source Available License Agreement - see [LICENSE](LICENSE)
