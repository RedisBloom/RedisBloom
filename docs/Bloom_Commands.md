# RedisBloom Bloom Filter Command Documentation

## BF.RESERVE

### Format:

```
BF.RESERVE {key} {error_rate} {capacity} [EXPANSION expansion] [NONSCALING]
```

### Description:

Creates an empty Bloom Filter with a given desired error ratio and initial capacity.

Though the filter can scale up by creating sub-filters, it will consume more memory
and CPU time than an equivalent filter that had the right capacity when initialized.

The number of hash functions is -log(error)/ln(2)^2
The number of bits per item is -log(error)/ln(2)

### Parameters:

* **key**: The key under which the filter is to be found
* **error_rate**: The desired probability for false positives. This should
    be a decimal value between 0 and 1. For example, for a desired false
    positive rate of 0.1% (1 in 1000), error_rate should be set to 0.001.
    The closer this number is to zero, the greater the memory consumption per
    item and the more CPU usage per operation.
* **capacity**: The number of entries you intend to add to the filter.
    Performance will begin to degrade after adding more items than this
    number. The actual degradation will depend on how far the limit has
    been exceeded. Performance will degrade linearly as the number of entries
    grow exponentially.

Optional parameters:

* **expansion**: If a new sub-filter is created, its size will be the size of the
    current filter multiplied by `expansion`.
    Default expansion value is 2. This means each subsequent sub-filter will be
    twice as large as the previous one.
* **NONSCALING**: Prevents the filter from creating additional sub-filters if
    initial capacity is reached. Non-scaling filters requires slightly less
    memory than their scaling counterparts.

### Complexity

O(1)

### Returns

OK on success, error otherwise.

## BF.ADD

### Format

```
BF.ADD {key} {item}
```

### Description

Adds an item to the Bloom Filter, creating the filter if it does not yet exist.

### Parameters

* **key**: The name of the filter
* **item**: The item to add

### Complexity

O(log N), where N is the number of stacked filters in the data structure.

### Returns

"1" if the item was newly inserted, or "0" if it may have existed previously.


## BF.MADD

### Format

```
BF.MADD {key} {item} [item...]
```

### Description

Adds one or more items to the Bloom Filter, creating the filter if it does not yet exist.
This command operates identically to `BF.ADD` except it allows multiple inputs and returns
multiple values.

### Parameters

* **key**: The name of the filter
* **items**: One or more items to add

### Complexity

O(log N), where N is the number of stacked filters in the data structure.

### Returns

An array of booleans (integers). Each element is either true or false depending
on whether the corresponding input element was newly added to the filter or may
have previously existed.

## BF.INSERT

```
BF.INSERT {key} [CAPACITY {cap}] [ERROR {error}] [EXPANSION expansion] [NOCREATE]
[NONSCALING] ITEMS {item...}
```

### Description

This command will add one or more items to the bloom filter, by default creating
it if it does not yet exist. There are several arguments which may be used to
modify this behavior.

### Parameters

* **key**: The name of the filter
* **CAPACITY**: If specified, should be followed by the desired capacity for the
    filter to be created. This parameter is ignored if the filter already exists.
    If the filter is automatically created and this parameter is absent, then the
    default capacity (specified at the module-level) is used. See `BF.RESERVE`
    for more information on the impacts of this value.
* **ERROR**: If specified, should be followed by the the error ratio of the newly
    created filter if it does not yet exist. If the filter is automatically
    created and `ERROR` is not specified then the default module-level error
    rate is used. See `BF.RESERVE` for more information on the format of this
    value.
* **expansion**: If a new sub-filter is created, its size will be the size of the
    current filter multiplied by `expansion`.
    Default expansion value is 2. This means each subsequent sub-filter will be
    twice as large as the previous one.
* **NOCREATE**: If specified, indicates that the filter should not be created if
    it does not already exist. If the filter does not yet exist, an error is
    returned rather than creating it automatically. This may be used where a strict
    separation between filter creation and filter addition is desired. It is an
    error to specify `NOCREATE` together with either `CAPACITY` or `ERROR`.
* **NONSCALING**: Prevents the filter from creating additional sub-filters if
    initial capacity is reached. Non-scaling filters requires slightly less
    memory than their scaling counterparts.
* **ITEMS**: Indicates the beginning of the items to be added to the filter. This
    parameter must be specified.

### Examples

Add three items to a filter, using default parameters if the filter does not already
exist:
```
BF.INSERT filter ITEMS foo bar baz
```

Add one item to a filter, specifying a capacity of 10000 to be used if it does not
already exist:
```
BF.INSERT filter CAPACITY 10000 ITEMS hello
```

Add 2 items to a filter, returning an error if the filter does not already exist

```
BF.INSERT filter NOCREATE ITEMS foo bar
```

### Complexity

O(log N), where N is the number of stacked filters in the data structure.

### Returns

An array of booleans (integers). Each element is either true or false depending
on whether the corresponding input element was newly added to the filter or may
have previously existed.

## BF.EXISTS

### Format

```
BF.EXISTS {key} {item}
```

### Description

Determines whether an item may exist in the Bloom Filter or not.

### Parameters

* **key**: the name of the filter
* **item**: the item to check for

### Complexity

O(log N), where N is the number of stacked filters in the data structure.

### Returns

"0" if the item certainly does not exist, "1" if the item may exist.


## BF.MEXISTS

### Format

```
BF.MEXISTS {key} {item} [item...]
```

### Description

Determines if one or more items may exist in the filter or not.

### Parameters

* **key**: name of the filter
* **items**: one or more items to check

### Complexity

O(log N), where N is the number of stacked filters in the data structure.

### Returns

An array of boolean values (actually integers). A true value means the
corresponding item may exist in the filter, while a false value means it does not.


## BF.SCANDUMP

### Format

```
BF.SCANDUMP {key} {iter}
```

### Description

Begins an incremental save of the bloom filter. This is useful for large bloom
filters which cannot fit into the normal `SAVE` and `RESTORE` model.

The first time this command is called, the value of `iter` should be 0. This
command will return successive `(iter, data)` pairs until `(0, NULL)` to
indicate completion.

A demonstration in python-flavored pseudocode:

```
chunks = []
iter = 0
while True:
    iter, data = BF.SCANDUMP(key, iter)
    if iter == 0:
        break
    else:
        chunks.append([iter, data])

# Load it back
for chunk in chunks:
    iter, data = chunk
    BF.LOADCHUNK(key, iter, data)
```

### Parameters

* **key** Name of the filter
* **iter** Iterator value. This is either 0, or the iterator from a previous
    invocation of this command

### Complexity

O(log N), where N is the number of stacked filters in the data structure.

### Returns

An array of _Iterator_ and _Data_. The Iterator is passed as input to the next
invocation of `SCANDUMP`. If _Iterator_ is 0, then it means iteration has
completed.

The iterator-data pair should also be passed to `LOADCHUNK` when restoring
the filter.

## BF.LOADCHUNK

### Format

```
BF.LOADCHUNK {key} {iter} {data}
```

### Description

Restores a filter previously saved using `SCANDUMP`. See the `SCANDUMP` command
for example usage.

This command will overwrite any bloom filter stored under `key`. Ensure that
the bloom filter will not be modified between invocations.

### Parameters

* **key** Name of the key to restore
* **iter** Iterator value associated with `data` (returned by `SCANDUMP`)
* **data** Current data chunk (returned by `SCANDUMP`)

### Complexity O

O(log N), where N is the number of stacked filters in the data structure.

### Returns

`OK` on success, or an error on failure.

## BF.INFO

### Format

```
BF.INFO {key}
```

### Description

Return information about `key`

### Parameters

* **key** Name of the key to restore

### Complexity O

O(1)

### Returns

```sql
127.0.0.1:6379> BF.INFO cf
1) Capacity
2) (integer) 1709
3) Size
4) (integer) 2200
5) Number of filters
6) (integer) 1
7) Number of items inserted
8) (integer) 0
9) Expansion rate
10) (integer) 1
```
