<img src="images/logo.svg" alt="logo" width="200"/>

# RedisBloom - Probablistic Datatypes Module for Redis

RedisBloom module provides four datatypes, a Scalable **Bloom Filter** and **Cuckoo Filter**, a **Count-Mins-Sketch** and a **Top-K**.
**Bloom and Cuckoo filters** are used to determine (with a given degree of certainty) whether an item is present or absent from a collection. While **Count-Min Sketch** is used to approximate count of items in sub-linear space and **Top-K** maintains a list of K most frequent items.

## Quick Start Guide
1. [Command references](#command-references)
1. [Launch RedisBloom with Docker](#launch-redisbloom-with-docker)
1. [Client libraries](#client-libraries)
1. [Webinars](#webinars)
1. [Past blog posts](#past-blog-posts)
1. [License](#license)

Note: You can also [build and load the module](#building-and-loading-redisbloom) yourself.

## Command references
Detailed command references can be found at [redisbloom.io](redisbloom.io).

### Launch RedisBloom with Docker
```
docker run -p 6379:6379 --name redis-redisbloom redislabs/rebloom:latest
```

## Building and Loading RedisBloom

In order to use this module, build it using `make` and load it into Redis.

### Loading

**Invoking redis with the module loaded**

```
$ redis-server --loadmodule /path/to/redisbloom.so
```

You can find a command reference in [Bloom\_Commands.md](Bloom_Commands.md)
and [Cuckoo\_Commands.md](Cuckoo_Commands.md)

## Building

In order to use this module, build it using `make` and load it into Redis.

### Module Options

You can adjust the default error ratio and the initial filter size (for bloom filters)
using the `ERROR_RATE` and `INITIAL_SIZE` options respectively when loading the
module, e.g.

```
$ redis-server --loadmodule /path/to/redisbloom.so INITIAL_SIZE 400 ERROR_RATE 0.004
```

The default error rate is `0.01` and the default initial capacity is `100`.

## Bloom vs. Cuckoo
Bloom Filters typically exhibit better performance and scalability when inserting
items (so if you're often adding items to your dataset then Bloom may be ideal),
whereas Cuckoo Filters are quicker on check operations and also allow deletions.

## Client libraries
| Project | Language | License | Author | URL |
| ------- | -------- | ------- | ------ | --- |
| redisbloom-py | Python | BSD | [Redis Labs](https://redislabs.com) | [GitHub](https://github.com/RedisBloom/redisbloom-py) |
| JReBloom | Java | BSD | [Redis Labs](https://redislabs.com) | [GitHub](https://github.com/RedisBloom/JReBloom) |
| rebloom | JavaScript | MIT | [Albert Team](https://cvitae.now.sh/) | [GitHub](https://github.com/albert-team/rebloom) |

## Webinars
1. [Probabilistic Data Structures - The most useful thing in Redis you probably aren't use](https://youtu.be/dq-0xagF7v8?t=102)

## Past blog posts
1. [ReBloom Quick Start Tutorial](https://docs.redislabs.com/latest/rs/getting-started/creating-database/rebloom/)
1. [Developing with Bloom Filters](https://docs.redislabs.com/latest/rs/developing/modules/bloom-filters/)
1. [ReBloom – Bloom Filter Datatype for Redis + Benchmark](https://redislabs.com/blog/rebloom-bloom-filter-datatype-redis/)


## License
Redis Source Available License Agreement - see [LICENSE](LICENSE)
