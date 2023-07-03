---
title: Redis Probabilistic
linkTitle: Probabilistic
description: Bloom filters and other probabilistic data structures for Redis
type: docs
---

[![Discord](https://img.shields.io/discord/697882427875393627?style=flat-square)](https://discord.gg/wXhwjCQ)
[![GitHub](https://img.shields.io/static/v1?label=&message=repository&color=5961FF&logo=github)](https://github.com/RedisBloom/RedisBloom/)

Redis Stack contains a set of useful probabilistic data structures. Probabilistic data structures allow developers to control the accuracy of returned results while gaining performance and reducing memory. 
These data structures are ideal for analyzing streaming data and large datasets.

Use these data structures to answer a set of common questions concerning data streams:

* **HyperLogLog**: How many unique values appeared so far in the data stream?
* **Bloom filter** and **Cuckoo filter**: Did the value _v_ already appear in the data stream?
* **Count-min sketch**: How many times did the value _v_ appear in the data stream?
* **Top-K**: What are the _k_ most frequent values in the data stream?
* **t-digest** can be used to answer these questions:
  - What fraction of the values in the data stream are smaller than a given value?
  - How many values in the data stream are smaller than a given value?
  - Which value is smaller than _p_ percent of the values in the data stream? (what is the _p_-percentile value)?
  - What is the mean value between the _p1_-percentile value and the _p2_-percentile value?
  - What is the value of the _n_-th smallest / largest value in the data stream? (what is the value with [reverse] rank _n_)?

Answering each of these questions accurately can require a huge amount of memory, but if you are willing to sacrifice accuracy, you can reduce the memory requirements drastically. Each of these data structures allows you to set a controllable tradeoff between accuracy and memory consumption.

## Bloom vs. Cuckoo filters
Bloom filters typically exhibit better performance and scalability when inserting
items (so if you're often adding items to your dataset, then a Bloom filter may be ideal).
Cuckoo filters are quicker on check operations and also allow deletions.

## About t-digest

Using t-digest is simple and straightforward:

* **Creating a sketch and adding observations**

  `TDIGEST.CREATE key [COMPRESSION compression]` initializes a new t-digest sketch (and error if such key already exists). The `COMPRESSION` argument is used to specify the tradeoff between accuracy and memory consumption. The default is 100. Higher values mean more accuracy.

  `TDIGEST.ADD key value...` adds new observations (floating-point values) to the sketch. You can repeat calling [TDIGEST.ADD](https://redis.io/commands/tdigest.add/) whenever new observations are available.

* **Estimating fractions or ranks by values**

  Use `TDIGEST.CDF key value...` to retrieve, for each input **value**, an estimation of the **fraction** of (observations **smaller** than the given value + half the observations equal to the given value).

  `TDIGEST.RANK key value...` is similar to [TDIGEST.CDF](https://redis.io/commands/tdigest.cdf/), but used for estimating the number of observations instead of the fraction of observations. More accurately it returns, for each input **value**, an estimation of the **number** of (observations **smaller** than a given value + half the observations equal to the given value).

  And lastly, `TDIGEST.REVRANK key value...` is similar to [TDIGEST.RANK](https://redis.io/commands/tdigest.rank/), but returns, for each input **value**, an estimation of the **number** of (observations **larger** than a given value + half the observations equal to the given value).

* **Estimating values by fractions or ranks**

  `TDIGEST.QUANTILE key fraction...` returns, for each input **fraction**, an estimation of the **value** (floating point) that is **smaller** than the given fraction of observations.

  `TDIGEST.BYRANK key rank...` returns, for each input **rank**, an estimation of the **value** (floating point) with that rank.

  `TDIGEST.BYREVRANK key rank...` returns, for each input **reverse rank**, an estimation of the **value** (floating point) with that reverse rank.

* **Estimating trimmed mean**

  Use `TDIGEST.TRIMMED_MEAN key lowFraction highFraction` to retrieve an estimation of the mean value between the specified fractions.

  This is especially useful for calculating the average value ignoring outliers. For example - calculating the average value between the 20th percentile and the 80th percentile.

* **Merging sketches**

  Sometimes it is useful to merge sketches. For example, suppose we measure latencies for 3 servers, and we want to calculate the 90%, 95%, and 99% latencies for all the servers combined.

  `TDIGEST.MERGE destKey numKeys sourceKey... [COMPRESSION compression] [OVERRIDE]` merges multiple sketches into a single sketch.

  If `destKey` does not exist - a new sketch is created.

  If `destKey` is an existing sketch, its values are merged with the values of the source keys. To override the destination key contents, use `OVERRIDE`.

* **Retrieving sketch information**

  Use `TDIGEST.MIN` key and `TDIGEST.MAX key` to retrieve the minimal and maximal values in the sketch, respectively.

  Both return nan when the sketch is empty.

  Both commands return accurate results and are equivalent to `TDIGEST.BYRANK key 0` and `TDIGEST.BYREVRANK key 0` respectively.

  Use `TDIGEST.INFO key` to retrieve some additional information about the sketch.

* **Resetting a sketch**

  `TDIGEST.RESET key` empties the sketch and re-initializes it.

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

t-digest
- [The _t_-digest: Efficient estimates of distributions](https://www.sciencedirect.com/science/article/pii/S2665963820300403)

## References
### Webinars
1. [Probabilistic Data Structures - The most useful thing in Redis you probably aren't using](https://youtu.be/dq-0xagF7v8?t=102)

### Blog posts
1. [RedisBloom Quick Start Tutorial](https://docs.redis.com/latest/modules/redisbloom/redisbloom-quickstart/)
1. [Developing with Bloom Filters](https://docs.redis.com/latest/modules/redisbloom/)
1. [RedisBloom on Redis Enterprise](https://redis.com/redis-enterprise/redis-bloom/)
1. [Probably and No: Redis, RedisBloom, and Bloom Filters](https://redis.com/blog/redis-redisbloom-bloom-filters/)
1. [RedisBloom – Bloom Filter Datatype for Redis](https://redis.com/blog/rebloom-bloom-filter-datatype-redis/)
1. [Count-Min Sketch: The Art and Science of Estimating Stuff](https://redis.com/blog/count-min-sketch-the-art-and-science-of-estimating-stuff/)
1. [Meet Top-K: an Awesome Probabilistic Addition to RedisBloom](https://redis.com/blog/meet-top-k-awesome-probabilistic-addition-redisbloom/)

## Mailing List / Forum
Got questions? Feel free to ask at the [Probabilistic forum](https://forum.redis.com/c/modules/redisbloom).

## License
Redis Probabilistic is licensed under the [Redis Source Available License 2.0 (RSALv2)](https://redis.com/legal/rsalv2-agreement) or the [Server Side Public License v1 (SSPLv1)](https://www.mongodb.com/licensing/server-side-public-license).
