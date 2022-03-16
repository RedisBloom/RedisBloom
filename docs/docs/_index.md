---
title: RedisBloom - Probabilistic Datatypes Module for Redis
linkTitle: RedisBloom
type: docs
---

<img src="images/logo.svg" alt="logo" width="200"/>

[![Forum](https://img.shields.io/badge/Forum-RedisBloom-blue)](https://forum.redis.com/c/modules/redisbloom)
[![Discord](https://img.shields.io/discord/697882427875393627?style=flat-square)](https://discord.gg/wXhwjCQ)

RedisBloom adds four probabilistic data structures to Redis: a scalable **Bloom filter**, a **cuckoo filter**, a **count-min sketch**, and a **top-k**. These data structures trade perfect accuracy for extreme memory efficiency, so they're especially useful for big data and streaming applications.

**Bloom and cuckoo filters** are used to determine, with a high degree of certainty, whether an element is a member of a set.

A **count-min sketch** is generally used to determine the frequency of events in a stream. You can query the count-min sketch get an estimate of the frequency of any given event.

A **top-k** maintains a list of _k_ most frequently seen items.

## Bloom vs. Cuckoo
Bloom filters typically exhibit better performance and scalability when inserting
items (so if you're often adding items to your dataset, then a Bloom filter may be ideal).
Cuckoo filters are quicker on check operations and also allow deletions.

## Academic sources
Bloom Filter
- [Space/Time Trade-offs in Hash Coding with Allowable Errors](http://www.dragonwins.com/domains/getteched/bbc/literature/Bloom70.pdf) by Burton H. Bloom.
- [Scalable Bloom Filters](https://haslab.uminho.pt/cbm/files/dbloom.pdf)

Cuckoo Filter
- [Cuckoo Filter: Practically Better Than Bloom](https://www.cs.cmu.edu/~dga/papers/cuckoo-conext2014.pdf)

Count-Min Sketch
- [An Improved Data Stream Summary: The Count-Min Sketch and its Applications](http://dimacs.rutgers.edu/~graham/pubs/papers/cm-full.pdf)

Top-K
- [HeavyKeeper: An Accurate Algorithm for Finding Top-k Elephant Flows.](https://yangtonghome.github.io/uploads/HeavyKeeper_ToN.pdf)

## References
### Webinars
1. [Probabilistic Data Structures - The most useful thing in Redis you probably aren't using](https://youtu.be/dq-0xagF7v8?t=102)

### Blog posts
1. [RedisBloom Quick Start Tutorial](https://docs.redis.com/latest/modules/redisbloom/redisbloom-quickstart/)
1. [Developing with Bloom Filters](https://docs.redis.com/latest/modules/redisbloom/)
1. [RedisBloom on Redis Enterprise](https://redis.com/redis-enterprise/redis-bloom/)
1. [Probably and No: Redis, RedisBloom, and Bloom Filters](https://redis.com/blog/redis-redisbloom-bloom-filters/)
1. [RedisBloom – Bloom Filter Datatype for Redis](https://redis.com/blog/rebloom-bloom-filter-datatype-redis/)

## Mailing List / Forum
Got questions? Feel free to ask at the [RedisBloom forum](https://forum.redis.com/c/modules/redisbloom).

## License
RedisBloom is licensed under the [Redis Source Available License Agreement](LICENSE)
