# ReBloom - Bloom Filter Module for Redis

This provides a scalable bloom filter as a Redis data type. Bloom filters
are probabilistic data structures that do a very good job at quickly
determining if something is contained within a set.

## Building

In order to use this module, build it using `make` and load it into Redis.

### Using

You can find a command reference in [docs/Commands.md](docs/Commands.md)


### Module Options

You can adjust the default error ratio and the initial filter size using
the `ERROR_RATE` and `INITIAL_SIZE` options respectively when loading the
module, e.g.

```
$ redis-server --loadmodule /path/to/rebloom.so INITIAL_SIZE 400 ERROR_RATE 0.004
```

The default error rate is `0.01` and the default initial capacity is `100`.
