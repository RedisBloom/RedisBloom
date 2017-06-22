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

### `BF.CREATE KEY ERROR ELEM1 ... ELEMN`

Create a new fixed bloom filter with the given elements.
The `ERROR` is a fractional number between 0 and 1 which is the desired maximum
error rate. A higher error rate will use less memory at the cost of more false
positives. A lower error rate will use more memory but reduce the chance of
a false positive.

If `ERROR` is 0 then the module will use the default of `0.01`.

```
> BF.CREATE newbf 0.05 foo bar baz mlem
```

### `BF.SET KEY ELEM1 ... ELEMN`
### `BF.SETNX KEY ELEM1 ... ELEMN`

Add elements to a bloom filter. If the bloom filter does not exist it will
be created for you.

It is an error to add elements to a bloom filter created with `BF.CREATE` and
an error will be returned if this is the case.

The `SETNX` variant does the same thing as `SET`, except it will fail if the
filter already exists.

```
> BF.SET somebf foo bar
(nil)
> BF.SET somebf baz
(nil)
> BF.SETNX newbf bar
(nil)
> BF.SETNX newbf baz
ERR already exists
```

### `BF.TEST KEY`

Check if an item (possible) exists in the set. This command will return either
1 or 0 depending on whether the item is potentially known or certainly unknown
to the filter

```
> BF.TEST somebf foo
1
> BF.TEST somebf lolwut
0
```

### Module Options

You can adjust the default error ratio and the initial filter size using
the `ERROR_RATE` and `INITIAL_SIZE` options respectively when loading the
module, e.g.

```
$ redis-server --loadmodule /path/to/rebloom.so INITIAL_SIZE 400 ERROR_RATE 0.004
```
