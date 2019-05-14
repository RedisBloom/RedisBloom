<img src="icon-probabilistic.png" alt="logo" width="200"/>

# RedisBloom - Probablistic Datatypes Module for Redis

This module provides two datatypes, a Scalable Bloom Filter, Cuckoo Filter and a Count-Mins-Sketch.
The first two datatypes are used to determine (with a given degree of certainty) whether
an item is present (or absent) from a collection and while the last is uesed to count the 
frequency of the different items in sub-linear space.


## Quick Start Guide
1. [Launch RedisBloom with Docker](#launch-redisbloom-with-docker)
1. [Use RedisBloom with redis-cli](#use-redisbloom-with-redis-cli)

Note: You can also [build and load the module](#building-and-loading-redisbloom) yourself.

You can find a command reference in [Bloom Commands.md](Bloom_Commands.md) and
[Cuckoo Commands](Cuckoo_Commands.md)


### Launch RedisBloom with Docker
```
docker run -p 6379:6379 --name redis-redisbloom redislabs/rebloom:latest
```

### Use RedisBloom with `redis-cli`
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

You can find a command reference in [Bloom\_Commands.md](Bloom_Commands.md)
and [Cuckoo\_Commands.md](Cuckoo_Commands.md)


## Building

In order to use this module, build it using `make` and load it into Redis.

### Module Options

You can adjust the default error ratio and the initial filter size (for bloom filters)
using the `ERROR_RATE` and `INITIAL_SIZE` options respectively when loading the
module, e.g.

```
$ redis-server --loadmodule /path/to/rebloom.so INITIAL_SIZE 400 ERROR_RATE 0.004
```

The default error rate is `0.01` and the default initial capacity is `100`.

## Bloom vs. Cuckoo

Bloom Filters typically exhibit better performance and scalability when inserting
items (so if you're often adding items to your dataset then Bloom may be ideal),
whereas Cuckoo Filters are quicker on check operations and also allow deletions.
