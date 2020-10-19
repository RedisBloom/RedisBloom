# RedisBloom Cuckoo Filter Documentation

Based on [Cuckoo Filter: Practically Better Than Bloom](
    https://www.eecs.harvard.edu/~michaelm/postscripts/cuckoo-conext2014.pdf) by
    Bin Fan, David G. Andersen and Michael Kaminsky.

## CF.RESERVE

### Format:

```
CF.RESERVE {key} {capacity} [BUCKETSIZE bucketSize] [MAXITERATIONS maxIterations]
[EXPANSION expansion]
```

Create a Cuckoo Filter as `key` with a single sub-filter for the initial amount
of `capacity` for items. Because of how Cuckoo Filters work, the filter is
likely to declare itself full before `capacity` is reached and therefore fill
rate will likely never reach 100%. The fill rate can be improved by using a
larger `bucketSize` at the cost of a higher error rate.
When the filter self-declare itself `full`, it will auto-expand by generating
additional sub-filters at the cost of reduced performance and increased error
rate. The new sub-filter is created with size of the previous sub-filter
multiplied by `expansion`.
Like bucket size, additional sub-filters grow the error rate linearly.
The size of the new sub-filter is the size of the last sub-filter multiplied by
`expansion`. The default value is 1.

The minimal false positive error rate is 2/255 ≈ 0.78% when bucket size of 1 is
used. Larger buckets increase the error rate linearly (for example, a bucket size
of 3 yields a 2.35% error rate) but improve the fill rate of the filter.

`maxIterations` dictates the number of attempts to find a slot for the incoming
fingerprint. Once the filter gets full, high `maxIterations` value will slow
down insertions. The default value is 20.

Unused capacity in prior sub-filters is automatically used when possible.
The filter can grow up to 32 times.

## Parameters:

* **key**: The key under which the filter is found.
* **capacity**: Estimated capacity for the filter. Capacity is rounded to the
next `2^n` number. The filter will likely not fill up to 100% of it's capacity.
Make sure to reserve extra capacity if you want to avoid expansions.

Optional parameters:

* **bucketSize**: Number of items in each bucket. A higher bucket size value
improves the fill rate but also causes a higher error rate and slightly slower
performance.
* **maxIterations**: Number of attempts to swap items between buckets before
declaring filter as full and creating an additional filter. A low value is
better for performance and a higher number is better for filter fill rate.
* **expansion**: When a new filter is created, its size is the size of the
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
as separate. You can use `CF.ADDNX` to only add the item if it does not
exist yet. Keep in mind that deleting an element inserted using `CF.ADDNX` may
cause false-negative errors.

### Parameters

* **key**: The name of the filter
* **item**: The item to add

### Complexity

O(n + i), where n is the number of `sub-filters` and i is `maxIterations`.
Adding items requires up to 2 memory accesses per `sub-filter`.
But as the filter fills up, both locations for an item might be full. The filter
attempts to `Cuckoo` swap items up to `maxIterations` times.

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
insert an element into the filter if its fingerprint already exists in order to
us the available capacity more efficiently. However, deleting
elements can introduce **false negative** error rate!

Note that this command is slower than `CF.ADD` because it first checks whether the
item exists.

### Parameters

* **key**: The name of the filter
* **item**: The item to add

### Complexity

O(n + i), where n is the number of `sub-filters` and i is `maxIterations`.
Adding items requires up to 2 memory accesses per `sub-filter`.
But as the filter fills up, both locations for an item might be full. The filter
attempts to `Cuckoo` swap items up to `maxIterations` times.

### Returns

"1" if the item was added to the filter, "0" if the item already exists.


## CF.INSERT

## CF.INSERTNX

Note: `CF.INSERTNX` is an advanced command that can have unintended impact if used
incorrectly.

```
CF.INSERT {key} [CAPACITY {cap}] [NOCREATE] ITEMS {item ...}
CF.INSERTNX {key} [CAPACITY {cap}] [NOCREATE] ITEMS {item ...}
```

### Description

Adds one or more items to a cuckoo filter, allowing the filter to be created
with a custom capacity if it does not exist yet.

This command is equivalent to a `CF.CHECK` + `CF.ADD` command. It does not
insert an element into the filter if its fingerprint already exists and
therefore better utilizes the available capacity. However, if you delete
elements it might introduce **false negative** error rate!

These commands offers more flexibility over the `ADD` and `ADDNX` commands, at
the cost of more verbosity.

### Parameters

* **key**: The name of the filter
* **CAPACITY**: If specified, should be followed by the desired capacity of the
    new filter, if this filter does not exist yet. If the filter already
    exists, then this parameter is ignored. If the filter does not exist yet
    and this parameter is *not* specified, then the filter is created with the
    module-level default capacity which is 1024. See `CF.RESERVE` for more
    information on cuckoo filter capacities.
* **NOCREATE**: If specified, prevents automatic filter creation if the filter
    does not exist. Instead, an error is returned if the filter does not
    already exist. This option is mutually exclusive with `CAPACITY`.
* **ITEMS**: Begin the list of items to add.

### Complexity

O(n + i), where n is the number of `sub-filters` and i is `maxIterations`.
Adding items requires up to 2 memory accesses per `sub-filter`.
But as the filter fills up, both locations for an item might be full. The filter
attempts to `Cuckoo` swap items up to `maxIterations` times.

### Returns

An array of booleans (as integers) corresponding to the items specified. Possible
values for each element are:

* `> 0` if the item was successfully inserted
* `0` if the item already existed *and* `INSERTNX` is used.
* `<0` if an error ocurred

Note that for `CF.INSERT`, the return value is always be an array of `>0` values,
unless an error occurs.

## CF.EXISTS

```
CF.EXISTS {key} {item}
```

Check if an item exists in a Cuckoo Filter

### Parameters

* **key**: The name of the filter
* **item**: The item to check for

### Complexity

O(n), where n is the number of `sub-filters`. Both alternative locations are
checked on all `sub-filters`.

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

O(n), where n is the number of `sub-filters`. Both alternative locations are
checked on all `sub-filters`.

### Returns

"1" if the item has been deleted, "0" if the item was not found.

## CF.COUNT

```
CF.COUNT {key} {item}
```

### Description

Returns the number of times an item may be in the filter. Because this is a
probabilistic data structure, this may not necessarily be accurate.

If you just want to know if an item exists in the filter, use `CF.EXISTS` because
it is more efficient for that purpose.

### Parameters

* **key**: The name of the filter
* **item**: The item to count

### Complexity

O(n), where n is the number of `sub-filters`. Both alternative locations are
checked on all `sub-filters`.

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
command returns successive `(iter, data)` pairs until `(0, NULL)`
indicates completion.

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

* **key**: Name of the filter
* **iter**: Iterator value. This is either 0, or the iterator from a previous
    invocation of this command

### Complexity

O(n), where n is the capacity.

### Returns

An array of _Iterator_ and _Data_. The Iterator is passed as input to the next
invocation of `SCANDUMP`. If _Iterator_ is 0, the iteration has
completed.

The iterator-data pair is also be passed to `LOADCHUNK` when restoring
the filter.

## CF.LOADCHUNK

### Format

```
CF.LOADCHUNK {key} {iter} {data}
```

### Description

Restores a filter previously saved using `SCANDUMP`. See the `SCANDUMP` command
for example usage.

This command overwrites any cuckoo filter stored under `key`. Make sure that
the cuckoo filter is not be modified between invocations.

### Parameters

* **key**: Name of the key to restore
* **iter**: Iterator value associated with `data` (returned by `SCANDUMP`)
* **data**: Current data chunk (returned by `SCANDUMP`)

### Complexity O

O(n), where n is the capacity.

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

* **key**: Name of the key to restore

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
