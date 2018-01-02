# ReBloom - Probablistic Datatypes Module for Redis

This module provides two datatypes, a Scalable Bloom Filter and a Cuckoo Filter.
These datatypes are used to determine (with a given degree of certainty) whether
an item is present (or absent) from a collection.


## Quick Start Guide
1. [Launch ReBloom with Docker](#launch-rebloom-with-docker)
1. [Use Rebloom with redis-cli](#use-rebloom-with-redis-cli)

Note: You can also [build and load the module](#building-and-loading-rebloom) yourself.

You can find a command reference in [Commands.md](Commands.md)


### Launch ReBloom with Docker
```
docker run -p 6379:6379 --name redis-rebloom redislabs/rebloom:latest
```

### Use ReBloom with `redis-cli`
```
docker exec -it redis-rebloom bash

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


## Building and Loading ReBloom

In order to use this module, build it using `make` and load it into Redis.

### Loading

**Invoking redis with the module loaded**

```
$ redis-server --loadmodule /path/to/rebloom.so
```

You can find a command reference in [docs/Commands.md](docs/Commands.md)


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