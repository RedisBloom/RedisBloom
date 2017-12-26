# ReBloom - Probablistic Datatypes Module for Redis

This module provides two datatypes, a Scalable Bloom Filter and a Cuckoo Filter.
These datatypes are used to determine (with a given degree of certainty) whether
an item is present (or absent) from a collection.

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