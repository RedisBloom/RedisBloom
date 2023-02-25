[![GitHub issues](https://img.shields.io/github/release/RedisLabsModules/redisbloom.svg)](https://github.com/RedisBloom/RedisBloom/releases/latest)
[![CircleCI](https://circleci.com/gh/RedisBloom/RedisBloom.svg?style=svg)](https://circleci.com/gh/RedisBloom/RedisBloom)
[![Dockerhub](https://img.shields.io/docker/pulls/redis/redis-stack-server?label=redis-stack-server)](https://img.shields.io/docker/pulls/redis/redis-stack-server)
[![codecov](https://codecov.io/gh/RedisBloom/RedisBloom/branch/master/graph/badge.svg)](https://codecov.io/gh/RedisBloom/RedisBloom)

# RedisBloom: Probabilistic Data Structures for Redis
[![Forum](https://img.shields.io/badge/Forum-RedisBloom-blue)](https://forum.redis.com/c/modules/redisbloom)
[![Discord](https://img.shields.io/discord/697882427875393627?style=flat-square)](https://discord.gg/wXhwjCQ)

<img src="docs/docs/images/logo.svg" alt="logo" width="300"/>

## Overview

RedisBloom adds a set of probabilistic data structures to Redis, including Bloom filter, Cuckoo filter, Count-min sketch, Top-K, and t-digest. Using this capability, you can query streaming data without needing to store all the elements of the stream. Probabilistic data structures each answer the following questions:

- Bloom filter and Cuckoo filter:
  -  Did value _v_ already appear in the data stream?
- Count-min sketch:
  - How many times did value _v_ appear in the data stream?
- Top-k:
  - What are the _k_ most frequent values in the data stream?
- t-digest:
  - Which fraction of the values in the data stream are smaller than a given value?
  - How many values in the data stream are smaller than a given value?
  - Which value is smaller than _p_ percent of the values in the data stream? (What is the _p_-percentile value?)
  - What is the mean value between the _p1_-percentile value and the _p2_-percentile value?
  - What is the value of the *n*ᵗʰ smallest/largest value in the data stream? (What is the value with [reverse] rank _n_?)

Answering each of these questions accurately can require a huge amount of memory, but you can lower the memory requirements drastically at the cost of reduced accuracy. Each of these data structures allows you to set a controllable trade-off between accuracy and memory consumption. In addition to having a smaller memory footprint, probabilistic data structures are generally much faster than accurate algorithms.

Redis Bloom is part of [Redis Stack](https://github.com/redis-stack).

## Getting started

1. [Launch RedisBloom with Docker](#1-launch-redisbloom-with-docker)
2. [Use RedisBloom with `redis-cli`](#2-use-redisbloom-with-redis-cli) or with [RedisInsight](https://redis.io/docs/ui/insight/)

Note: You can also [build and load the module](#building-and-loading-redisbloom) yourself.

### 1. Launch RedisBloom with Docker
```
docker run -d --name redis-stack-server -p 6379:6379 redis/redis-stack-server:latest
```

### 2. Use RedisBloom with `redis-cli`
```
docker exec -it redis/redis-stack-server bash

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

To build RedisBloom, ensure you have the proper git submodules, and afterwards run `make` in the project's directory.

```
git submodule update --init --recursive
make
```

If the build is successful, you'll have a shared library called `redisbloom.so`.

To load the library, pass its path to the `loadmodule` directive when starting `redis-server`:
```
$ redis-server --loadmodule /path/to/redisbloom.so
```

## Client libraries
| Project | Language | License | Author | Stars | Package | Comment |
| ------- | -------- | ------- | ------ | ----- | ------- | ------- |
| [jedis][jedis-url] | Java | MIT | [Redis][redis-url] | ![Stars][jedis-stars] | [Maven][jedis-package]||
| [redis-py][redis-py-url] | Python | MIT | [Redis][redis-url] | ![Stars][redis-py-stars] | [pypi][redis-py-package]||
| [node-redis][node-redis-url] | Node.JS | MIT | [Redis][redis-url] | ![Stars][node-redis-stars] | [npm][node-redis-package]||
| [nredisstack][nredisstack-url] | .NET | MIT | [Redis][redis-url] | ![Stars][nredisstack-stars] | [nuget][nredisstack-package]||
| redisbloom-go | Go | BSD | [Redis](https://redis.com) |  ![Stars](https://img.shields.io/github/stars/RedisBloom/redisbloom-go.svg?style=social&amp;label=Star&amp;maxAge=2592000) | [GitHub](https://github.com/RedisBloom/redisbloom-go) ||
| rueidis | Go | Apache License 2.0 | [Rueian](https://github.com/rueian) |  ![Stars](https://img.shields.io/github/stars/rueian/rueidis.svg?style=social&amp;label=Star&amp;maxAge=2592000) | [GitHub](https://github.com/rueian/rueidis) ||
| rebloom | JavaScript | MIT | [Albert Team](https://cvitae.now.sh/) | ![Stars](https://img.shields.io/github/stars/albert-team/rebloom.svg?style=social&amp;label=Star&amp;maxAge=2592000) |[GitHub](https://github.com/albert-team/rebloom) ||
| phpredis-bloom | PHP | MIT | [Rafa Campoy](https://github.com/averias) | ![Stars](https://img.shields.io/github/stars/averias/phpredis-bloom.svg?style=social&amp;label=Star&amp;maxAge=2592000) | [GitHub](https://github.com/averias/phpredis-bloom) ||
| phpRebloom | PHP | MIT | [Alessandro Balasco](https://github.com/palicao) | ![Stars](https://img.shields.io/github/stars/palicao/phprebloom.svg?style=social&amp;label=Star&amp;maxAge=2592000) | [GitHub](https://github.com/palicao/phpRebloom) ||
| vertx-redis-client | Java | Apache License 2.0 | [Eclipse Vert.x](https://github.com/vert-x3) | ![Stars](https://img.shields.io/github/stars/vert-x3/vertx-redis-client.svg?style=social&amp;label=Star&amp;maxAge=2592000) | [GitHub](https://github.com/vert-x3/vertx-redis-client) ||
| rustis | Rust | MIT | [Dahomey Technologies](https://github.com/dahomey-technologies) | ![Stars](https://img.shields.io/github/stars/dahomey-technologies/rustis.svg?style=social&amp;label=Star&amp;maxAge=2592000) | [GitHub](https://github.com/dahomey-technologies/rustis) |

[redis-url]: https://redis.com

[redis-py-url]: https://github.com/redis/redis-py
[redis-py-stars]: https://img.shields.io/github/stars/redis/redis-py.svg?style=social&amp;label=Star&amp;maxAge=2592000
[redis-py-package]: https://pypi.python.org/pypi/redis

[jedis-url]: https://github.com/redis/jedis
[jedis-stars]: https://img.shields.io/github/stars/redis/jedis.svg?style=social&amp;label=Star&amp;maxAge=2592000
[Jedis-package]: https://search.maven.org/artifact/redis.clients/jedis

[nredisstack-url]: https://github.com/redis/nredisstack
[nredisstack-stars]: https://img.shields.io/github/stars/redis/nredisstack.svg?style=social&amp;label=Star&amp;maxAge=2592000
[nredisstack-package]: https://www.nuget.org/packages/nredisstack/

[node-redis-url]: https://github.com/redis/node-redis
[node-redis-stars]: https://img.shields.io/github/stars/redis/node-redis.svg?style=social&amp;label=Star&amp;maxAge=2592000
[node-redis-package]: https://www.npmjs.com/package/redis

## Documentation
Documentation and full command reference at [redisbloom.io](http://redisbloom.io).

## Mailing List / Forum
Got questions? Feel free to ask at the [RedisBloom mailing list](https://forum.redis.com/c/modules/redisbloom).

## License
RedisBloom is licensed under the [Redis Source Available License 2.0 (RSALv2)](https://redis.com/legal/rsalv2-agreement) or the [Server Side Public License v1 (SSPLv1)](https://www.mongodb.com/licensing/server-side-public-license).
