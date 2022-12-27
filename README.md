[![GitHub issues](https://img.shields.io/github/release/RedisLabsModules/redisbloom.svg)](https://github.com/RedisBloom/RedisBloom/releases/latest)
[![CircleCI](https://circleci.com/gh/RedisBloom/RedisBloom.svg?style=svg)](https://circleci.com/gh/RedisBloom/RedisBloom)
[![Dockerhub](https://img.shields.io/docker/pulls/redis/redis-stack-server?label=redis-stack-server)](https://img.shields.io/docker/pulls/redis/redis-stack-server)
[![codecov](https://codecov.io/gh/RedisBloom/RedisBloom/branch/master/graph/badge.svg)](https://codecov.io/gh/RedisBloom/RedisBloom)
[![Total alerts](https://img.shields.io/lgtm/alerts/g/RedisBloom/RedisBloom.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/RedisBloom/RedisBloom/alerts/)

# RedisBloom: Probabilistic Data Structures for Redis
[![Forum](https://img.shields.io/badge/Forum-RedisBloom-blue)](https://forum.redis.com/c/modules/redisbloom)
[![Discord](https://img.shields.io/discord/697882427875393627?style=flat-square)](https://discord.gg/wXhwjCQ)

The RedisBloom module provides four data structures: a scalable **Bloom filter**,  a **cuckoo filter**, a **count-min sketch**, and a **top-k**. These data structures trade perfect accuracy for extreme memory efficiency, so they're especially useful for big data and streaming applications.

**Bloom and cuckoo filters** are used to determine, with a high degree of certainty, whether an element is a member of a set.

A **count-min sketch** is generally used to determine the frequency of events in a stream. You can query the count-min sketch get an estimate of the frequency of any given event.

A **top-k** maintains a list of _k_ most frequently seen items.

## Quick Start Guide
1. [Launch RedisBloom with Docker](#launch-redisbloom-with-docker)
1. [Use RedisBloom with `redis-cli`](#use-redisbloom-with-redis-cli)

Note: You can also [build and load the module](#building-and-loading-redisbloom) yourself.

### 1. Launch with Docker
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
| [node-redis][node-redis-url] | Python | MIT | [Redis][redis-url] | ![Stars][node-redis-stars] | [pypi][node-redis-package]||
| [nredisstack][nredisstack-url] | Python | MIT | [Redis][redis-url] | ![Stars][nredisstack-stars] | [pypi][nredisstack-package]||
| redisbloom-go | Go | BSD | [Redis](https://redis.com) |  ![Stars](https://img.shields.io/github/stars/RedisBloom/redisbloom-go.svg?style=social&amp;label=Star&amp;maxAge=2592000) | [GitHub](https://github.com/RedisBloom/redisbloom-go) ||
| rueidis | Go | Apache License 2.0 | [Rueian](https://github.com/rueian) |  ![Stars](https://img.shields.io/github/stars/rueian/rueidis.svg?style=social&amp;label=Star&amp;maxAge=2592000) | [GitHub](https://github.com/rueian/rueidis) ||
| rebloom | JavaScript | MIT | [Albert Team](https://cvitae.now.sh/) | ![Stars](https://img.shields.io/github/stars/albert-team/rebloom.svg?style=social&amp;label=Star&amp;maxAge=2592000) |[GitHub](https://github.com/albert-team/rebloom) ||
| phpredis-bloom | PHP | MIT | [Rafa Campoy](https://github.com/averias) | ![Stars](https://img.shields.io/github/stars/averias/phpredis-bloom.svg?style=social&amp;label=Star&amp;maxAge=2592000) | [GitHub](https://github.com/averias/phpredis-bloom) ||
| phpRebloom | PHP | MIT | [Alessandro Balasco](https://github.com/palicao) | ![Stars](https://img.shields.io/github/stars/palicao/phprebloom.svg?style=social&amp;label=Star&amp;maxAge=2592000) | [GitHub](https://github.com/palicao/phpRebloom) ||
| vertx-redis-client | Java | Apache License 2.0 | [Eclipse Vert.x](https://github.com/vert-x3) | ![Stars](https://img.shields.io/github/stars/vert-x3/vertx-redis-client.svg?style=social&amp;label=Star&amp;maxAge=2592000) | [GitHub](https://github.com/vert-x3/vertx-redis-client) ||

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
