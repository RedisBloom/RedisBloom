# ReBloom - Bloom Filter Module for Redis

This provides a scalable bloom filter as a Redis data type. Bloom filters
are probabilistic data structures that do a very good job at quickly
determining if something is contained within a set.

## Building

In order to use this module, build it using `make` and load it into Redis.

## API

ReBloom allows you to create either a fixed bloom filter (which cannot be
added to) or a scalable bloom filter (which allows the addition of more
elements after it's been created).

In theory a fixed bloom filter is quicker than a scalable one because the
cost of searching for an element will remain linear across its lifetime, whereas
a scalable bloom filter occupies more memory and lookups may degrade slightly
over time as more elements are added.

### `BF.RESERVE KEY ERROR CAPACITY`

Create an empty bloom filter with a custom error rate and initial capacity.
This command is useful if you intend to create a large bloom filter, so that
the initial capacity can be optimized for the ideal memory usage and CPU
performance.

```
127.0.0.1:6379> BF.RESERVE test 0.0001 10000
OK
```

### `BF.SET KEY ELEM1 ... ELEMN`

Add elements to a bloom filter. If the bloom filter does not exist it will
be created for you.

The response is an array of integers that may be treated as booleans. A true
value means that the corresponding input element did not exist, while a false
value means that it (or its hashed equivalent) did exist.

```
127.0.0.1:6379> BF.SET test foo
1) (integer) 1
127.0.0.1:6379> BF.SET test foo bar baz
1) (integer) 0
2) (integer) 1
3) (integer) 1
127.0.0.1:6379> BF.SET test new1 new2 new3
1) (integer) 1
2) (integer) 1
3) (integer) 1
```

### `BF.TEST KEY`

Check if an item (possible) exists in the set. This command will return either
1 or 0 depending on whether the item is potentially known or certainly unknown
to the filter

```
127.0.0.1:6379> BF.TEST test foo
(integer) 1
127.0.0.1:6379> BF.TEST test bar
(integer) 1
127.0.0.1:6379> BF.TEST test nonexist
(integer) 0
```

### Module Options

You can adjust the default error ratio and the initial filter size using
the `ERROR_RATE` and `INITIAL_SIZE` options respectively when loading the
module, e.g.

```
$ redis-server --loadmodule /path/to/rebloom.so INITIAL_SIZE 400 ERROR_RATE 0.004
```

The default error rate is `0.01` and the default initial capacity is `100`.