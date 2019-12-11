# RedisBloom Cuckoo Filter Documentation

Based on [Cuckoo Filter: Practically Better Than Bloom](
    https://www.eecs.harvard.edu/~michaelm/postscripts/cuckoo-conext2014.pdf) paper by 
    Bin Fan, David G. Andersen and Michael Kaminsky.

## CF.RESERVE

### Format:

```
CF.RESERVE {key} {capacity} [BUCKETSIZE bucketSize] [MAXITERATIONS maxIterations] [EXPANSION expansion]
```

Create a Cuckoo Filter as `key` with an initial amount of `capacity` for items.
Because of how Cuckoo Filters work, the filter is likely to declare itself full
before `capacity` is reached and therefore fill rate will likely never reach
100%. The fill rate can be improved by using a larger `bucketSize` at the cost
of a higher error rate.

The minimal false positive error rate is 2/255 ≈ 0.78% when bucket size of 1 is
used. Larger buckets increase the error rate linearly (ex. bucket size of 3
will yield 2.35% error rate) but improve the filter's fill-rate.

The filter will auto-expand and generate a sub-filter at the cost of reduced
performance and increased error rate, when the filter declares itself full.
Like bucket size, additional sub-filters grow the error linearly.
The size of the new sub-filter is the size of the last sub-filter multiplied by
`expansion`. The default value is 1.

`maxIterations` dictates the number of attempts to find a slot for the incoming
fingerprint. Once the filter gets full, high `maxIterations` value will slow
down insertions. The default value is 20.

Unused capacity in prior sub-filters is automatically utilized when possible.
The filter can grows up to 32 times.

## Parameters:

* **key**: The key under which the filter is to be found.
* **capacity**: Estimated capacity for the filter. Capacity is rounded to the
next `2^n` number. The filter will likely not fill up to 100% of it's capacity.
Make sure to reserve extra capacity if you want to avoid expansions.

Optional parameters:

* **bucketSize**: Number of items in each bucket. Higher bucket size value
improves the fill rate but result in a higher error rate and slightly slower
operation speed.
* **maxIterations**: Number of attempts to swap items between buckets before
declaring filter as full and creating an additional filter. A low value is
better for performance while a higher number is better for filter fill rate.
* **expansion**: When a new filter is created, its size will be the size of the
current filter multiplied by `expansion`. Expansion is rounded to the next
`2^n` number.

### Complexity

O(1)

### Returns

OK on success, error otherwise

## CF.ADD

```
CF.ADD {key} {item}
```

### Description

Adds an item to the cuckoo filter, creating the filter if it does not exist.

Cuckoo filters can contain the same item multiple times, and consider each insert
to be separate. You can use `CF.ADDNX` to only add the item if it does not yet
exist. Keep in mind that deleting an element inserted using `CF.ADDNX` may
introduce false-negative errors.

### Parameters

* **key**: The name of the filter
* **item**: The item to add

### Complexity

O(sub-filters + maxIterations). Adding items usually requires 2 memory accesses.
Yet, as the filter fills up, both locations for an item might be full. The filter
will attempt to `Cuckoo` swap items up to `maxIterations` times.

### Returns

"1" on success, error otherwise.


## CF.ADDNX

Note: `CF.ADDNX` is an advanced command that might have implications if used
incorrectly.

```
CF.ADDNX {key} {item}
```

### Description

Adds an item to a cuckoo filter if the item did not exist previously.
See documentation on `CF.ADD` for more information on this command.

This command is equivalent to a `CF.CHECK` + `CF.ADD` command. It does not
insert an element into the filter if its fingerprint already exists and
therefore better utilizes the available capacity. However, if you delete
elements it might introduce **false negative** error rate!

Note that this command is slower than `CF.ADD` as it first checks whether the
item exists.

### Parameters

* **key**: The name of the filter
* **item**: The item to add

### Complexity

O(sub-filters + maxIterations). Adding items usually requires 2 memory accesses.
Yet, as the filter fills up, both locations for an item might be full. The filter
will attempt to `Cuckoo` swap items up to `maxIterations` times.

### Returns

"1" if the item was added to the filter, "0" if the item already exists.


## CF.INSERT

## CF.INSERTNX

Note: `CF.INSERTNX` is an advanced command that might have implications if used
incorrectly.

```
CF.INSERT {key} [CAPACITY {cap}] [NOCREATE] ITEMS {item ...}
CF.INSERTNX {key} [CAPACITY {cap}] [NOCREATE] ITEMS {item ...}
```

### Description
Adds one or more items to a cuckoo filter, allowing the filter to be created
with a custom capacity if it does not yet exist.

This command is equivalent to a `CF.CHECK` + `CF.ADD` command. It does not
insert an element into the filter if its fingerprint already exists and
therefore better utilizes the available capacity. However, if you delete
elements it might introduce **false negative** error rate!

These commands offers more flexibility over the `ADD` and `ADDNX` commands, at
the cost of more verbosity.

### Parameters

* **key**: The name of the filter
* **CAPACITY**: If specified, should be followed by the desired capacity of the
    new filter, if this filter does not yet exist. If the filter already
    exists, then this parameter is ignored. If the filter does not yet exist
    and this parameter is *not* specified, then the filter is created with the
    module-level default capacity. See `CF.RESERVE` for more information on
    cuckoo filter capacities.
* **NOCREATE**: If specified, prevent automatic filter creation if the filter
    does not exist. Instead, an error will be returned if the filter does not
    already exist. This option is mutually exclusive with `CAPACITY`.
* **ITEMS**: Begin the list of items to add.

### Complexity

O(sub-filters + maxIterations). Adding items usually requires 2 memory accesses.
Yet, as the filter fills up, both locations for an item might be full. The filter
will attempt to `Cuckoo` swap items up to `maxIterations` times.

### Returns

An array of booleans (as integers) corresponding to the items specified. Possible
values for each element are:

* `> 0` if the item was successfully inserted
* `0` if the item already existed *and* `INSERTNX` is used.
* `<0` if an error ocurred

Note that for `CF.INSERT`, unless an error occurred, the return value will always
be an array of `>0` values.

## CF.EXISTS

```
CF.EXISTS {key} {item}
```

Check if an item exists in a Cuckoo Filter

### Parameters

* **key**: The name of the filter
* **item**: The item to check for

### Complexity

O(sub-filters), Both alternative locations will be checked on all `sub-filters`.

### Returns

"0" if the item certainly does not exist, "1" if the item may exist. Because this
is a probabilistic data structure, false positives (but not false negatives) may
be returned.

## CF.DEL

```
CF.DEL {key} {item}
```
### Description

Deletes an item once from the filter. If the item exists only once, it will be
removed from the filter. If the item was added multiple times, it will still be
present.

!!! danger ""
    Deleting elements that are not in the filter may delete a different item,
    resulting in false negatives!

### Parameters

* **key**: The name of the filter
* **item**: The item to delete from the filter

### Complexity

O(sub-filters), Both alternative locations will be checked on all `sub-filters`.

### Returns

"1" if the item has been deleted, "0" if the item was not found.

## CF.COUNT

```
CF.COUNT {key} {item}
```

### Description

Returns the number of times an item may be in the filter. Because this is a
probabilistic data structure, this may not necessarily be accurate.

If you simply want to know if an item exists in the filter, use `CF.EXISTS`, as
that function is more efficient for that purpose.

### Parameters

* **key**: The name of the filter
* **item**: The item to count

### Complexity

O(sub-filters), Both alternative locations will be checked on all `sub-filters`.

### Returns

The number of times the item exists in the filter

## CF.SCANDUMP

### Format

```
CF.SCANDUMP {key} {iter}
```

### Description

Begins an incremental save of the cuckoo filter. This is useful for large cuckoo
filters which cannot fit into the normal `SAVE` and `RESTORE` model.

The first time this command is called, the value of `iter` should be 0. This
command will return successive `(iter, data)` pairs until `(0, NULL)` to
indicate completion.

A demonstration in python-flavored pseudocode:

```
chunks = []
iter = 0
while True:
    iter, data = CF.SCANDUMP(key, iter)
    if iter == 0:
        break
    else:
        chunks.append([iter, data])

# Load it back
for chunk in chunks:
    iter, data = chunk
    CF.LOADCHUNK(key, iter, data)
```

### Parameters

* **key** Name of the filter
* **iter** Iterator value. This is either 0, or the iterator from a previous
    invocation of this command

### Complexity

O(log N)

### Returns

An array of _Iterator_ and _Data_. The Iterator is passed as input to the next
invocation of `SCANDUMP`. If _Iterator_ is 0, then it means iteration has
completed.

The iterator-data pair should also be passed to `LOADCHUNK` when restoring
the filter.

## CF.LOADCHUNK

### Format

```
CF.LOADCHUNK {key} {iter} {data}
```

### Description

Restores a filter previously saved using `SCANDUMP`. See the `SCANDUMP` command
for example usage.

This command will overwrite any cuckoo filter stored under `key`. Ensure that
the cuckoo filter will not be modified between invocations.

### Parameters

* **key** Name of the key to restore
* **iter** Iterator value associated with `data` (returned by `SCANDUMP`)
* **data** Current data chunk (returned by `SCANDUMP`)

### Complexity O

O(log N)

### Returns

`OK` on success, or an error on failure.


## CF.INFO

### Format

```
CF.INFO {key}
```

### Description

Return information about `key`

### Parameters

* **key** Name of the key to restore

### Complexity O

O(1)

### Returns

```sql
127.0.0.1:6379> CF.INFO cf
 1) Size
 2) (integer) 1080
 3) Number of buckets
 4) (integer) 512
 5) Number of filter
 6) (integer) 1
 7) Number of items inserted
 8) (integer) 0
 9) Number of items deleted
10) (integer) 0
11) Bucket size
12) (integer) 2
13) Expansion rate
14) (integer) 1
15) Max iteration
16) (integer) 20
```
