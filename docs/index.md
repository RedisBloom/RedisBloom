<img src="images/logo.svg" alt="logo" width="200"/>

# RedisBloom - Probabilistic Datatypes Module for Redis
[![Mailing List](https://img.shields.io/badge/Mailing%20List-RedisBloom-blue)](https://groups.google.com/forum/#!forum/redisbloom)
[![Gitter](https://badges.gitter.im/RedisLabs/RedisBloom.svg)](https://gitter.im/RedisLabs/RedisBloom?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge)

RedisBloom module provides four datatypes, a Scalable **Bloom Filter** and **Cuckoo Filter**, a **Count-Mins-Sketch** and a **Top-K**.
**Bloom and Cuckoo filters** are used to determine (with a given degree of certainty) whether an item is present or absent from a collection. While **Count-Min Sketch** is used to approximate count of items in sub-linear space and **Top-K** maintains a list of K most frequent items.

## Quick Start Guide
1. [Quick Start](Quick_Start.md)
1. [Command references](#command-references)
1. [Client libraries](#client-libraries)
1. [References](#references)
1. [License](#license)

## Command references
Detailed command references for each data structure:

* [Bloom Filter](Bloom_Commands.md)
* [Cuckoo Filter](Cuckoo_Commands.md)
* [Count-Min Sketch](CountMinSketch_Commands.md)
* [Top-K](TopK_Commands.md)

## Bloom vs. Cuckoo
Bloom Filters typically exhibit better performance and scalability when inserting
items (so if you're often adding items to your dataset then Bloom may be ideal),
whereas Cuckoo Filters are quicker on check operations and also allow deletions.

## Client libraries
Each driver comes with its own documentation in the Readme of the driver repo.

| Project | Language | License | Author | URL |
| ------- | -------- | ------- | ------ | --- |
| redisbloom-py | Python | BSD | [Redis Labs](https://redislabs.com) | [GitHub](https://github.com/RedisBloom/redisbloom-py) |
| JReBloom | Java | BSD | [Redis Labs](https://redislabs.com) | [GitHub](https://github.com/RedisBloom/JReBloom) |
| rebloom | JavaScript | MIT | [Albert Team](https://cvitae.now.sh/) | [GitHub](https://github.com/albert-team/rebloom) |
| phpredis-bloom | PHP | MIT | [Rafa Campoy](https://github.com/averias) | [GitHub](https://github.com/averias/phpredis-bloom) |



## References
### Webinars
1. [Probabilistic Data Structures - The most useful thing in Redis you probably aren't use](https://youtu.be/dq-0xagF7v8?t=102)

### Past blog posts
1. [ReBloom Quick Start Tutorial](https://docs.redislabs.com/latest/rs/getting-started/creating-database/rebloom/)
1. [Developing with Bloom Filters](https://docs.redislabs.com/latest/rs/developing/modules/bloom-filters/)
1. [ReBloom – Bloom Filter Datatype for Redis + Benchmark](https://redislabs.com/blog/rebloom-bloom-filter-datatype-redis/)
1. [Meet Top-K: an Awesome Probabilistic Addition to RedisBloom](https://redislabs.com/blog/meet-top-k-awesome-probabilistic-addition-redisbloom/)

## Mailing List / Forum
Got questions? Feel free to ask at the [RedisBloom mailing list](https://groups.google.com/forum/#!forum/redisbloom).

## License
Redis Source Available License Agreement - see [LICENSE](https://raw.githubusercontent.com/RedisBloom/RedisBloom/master/LICENSE)
