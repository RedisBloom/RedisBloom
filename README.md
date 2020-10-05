[![GitHub issues](https://img.shields.io/github/release/RedisLabsModules/redisbloom.svg)](https://github.com/RedisBloom/RedisBloom/releases/latest)
[![CircleCI](https://circleci.com/gh/RedisBloom/RedisBloom.svg?style=svg)](https://circleci.com/gh/RedisBloom/RedisBloom)
[![Docker Cloud Build Status](https://img.shields.io/docker/cloud/build/redislabs/rebloom.svg)](https://hub.docker.com/r/redislabs/rebloom/builds/)
[![codecov](https://codecov.io/gh/RedisBloom/RedisBloom/branch/master/graph/badge.svg)](https://codecov.io/gh/RedisBloom/RedisBloom)
[![Total alerts](https://img.shields.io/lgtm/alerts/g/RedisBloom/RedisBloom.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/RedisBloom/RedisBloom/alerts/)

# RedisBloom: Probabilistic Data Structures for Redis
[![Forum](https://img.shields.io/badge/Forum-RedisBloom-blue)](https://forum.redislabs.com/c/modules/redisbloom)
[![Gitter](https://badges.gitter.im/RedisLabs/RedisBloom.svg)](https://gitter.im/RedisLabs/RedisBloom?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge)

The RedisBloom module provides four data structures: a scalable **Bloom filter**,  a **cuckoo filter**, a **count-min sketch**, and a **top-k**. These data structures trade perfect accuracy for extreme memory efficiency, so they're especially useful for big data and streaming applications.

**Bloom and cuckoo filters** are used to determine, with a high degree of certainty, whether an element is a member of a set.

A **count-min sketch** is generally used to determine the frequency of events in a stream. You can query the count-min sketch get an estimate of the frequency of any given event.

A **top-k** maintains a list of _k_ most frequently seen items.

## Quick Start Guide
1. [Launch RedisBloom with Docker](#launch-redisbloom-with-docker)
1. [Use RedisBloom with `redis-cli`](#use-redisbloom-with-redis-cli)

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

Find out whether an item exists in the filter:
```
# 127.0.0.1:6379> BF.EXISTS newFilter foo
(integer) 1
```

In this case, `1` means that the `foo` is most likely in the set represented by `newFilter`. But recall that false positives are possible with Bloom filters.

```
# 127.0.0.1:6379> BF.EXISTS newFilter bar
(integer) 0
```

A value `0` means that `bar` is definitely not in the set. Bloom filters do not allow for false negatives.

## Building and Loading RedisBloom
To use RedisBloom, first build its shared library by running `make`. If the build is successful, you'll have a shared library called `redisbloom.so`.

To load the library, pass its path to the `loadmodule` directive when starting `redis-server`:
```
$ redis-server --loadmodule /path/to/redisbloom.so
```

## Client libraries
| Project | Language | License | Author | URL |
| ------- | -------- | ------- | ------ | --- |
| redisbloom-py | Python | BSD | [Redis Labs](https://redislabs.com) | [GitHub](https://github.com/RedisBloom/redisbloom-py) |
| JReBloom | Java | BSD | [Redis Labs](https://redislabs.com) | [GitHub](https://github.com/RedisBloom/JReBloom) |
| redisbloom-go | Go | BSD | [Redis Labs](https://redislabs.com) | [GitHub](https://github.com/RedisBloom/redisbloom-go) |
| rebloom | JavaScript | MIT | [Albert Team](https://cvitae.now.sh/) | [GitHub](https://github.com/albert-team/rebloom) |
| phpredis-bloom | PHP | MIT | [Rafa Campoy](https://github.com/averias) | [GitHub](https://github.com/averias/phpredis-bloom) |
| phpRebloom | PHP | MIT | [Alessandro Balasco](https://github.com/palicao) | [GitHub](https://github.com/palicao/phpRebloom) |
| StackExchange.Redis (extensions) | .NET | MIT | [StackExchange](https://github.com/StackExchange/StackExchange.Redis) | [GitHub](https://gist.github.com/naile/96de4e9548c7b5fd6c0614009ffec755) |

## Documentation
Documentation and full command reference at [redisbloom.io](http://redisbloom.io).

## Mailing List / Forum
Got questions? Feel free to ask at the [RedisBloom mailing list](https://forum.redislabs.com/c/modules/redisbloom).

## License
RedisBloom is licensed under the [Redis Source Available License Agreement](LICENSE)
