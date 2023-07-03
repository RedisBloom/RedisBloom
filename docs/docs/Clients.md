---
title: "Client Libraries"
linkTitle: "Clients"
weight: 4
description: >
    Redis Stack has several client libraries, written by the module authors and community members - abstracting the API in different programming languages.
    While it is possible and simple to use the raw Redis commands API, in most cases it's easier to just use a client library abstracting it.
---

## Currently available Libraries

| Project | Language | License | Author | Stars | Package |
| ------- | -------- | ------- | ------ | ----- | ------- |
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