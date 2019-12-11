# RedisBloom Bloom Filter Command Documentation

Based on [Space/Time Trade-offs in Hash Coding with Allowable Errors](
    http://www.dragonwins.com/domains/getteched/bbc/literature/Bloom70.pdf) paper
    by Burton H. Bloom.

## BF.RESERVE

### Format:

```
BF.RESERVE {key} {error_rate} {capacity} [EXPANSION expansion] [NONSCALING]
```

### Description:

Creates an empty Bloom Filter for the initial capacity requested with an upper
bound `error_rate`. By default, the filter will auto-scale when `capacity` is
reached.

Though the filter can scale up by creating sub-filters, it is recommended to
reserve enough capacity since maintaining and querying sub-filters requires
more memory and CPU time than an equivalent filter that had the right capacity
on creation time.

The number of hash functions is -log(error)/ln(2)^2.
The number of bits per item is -log(error)/ln(2) ≈ 1.44.

* **1%**    error rate requires 7  hash functions and 10.08 bits per item.
* **0.1%**  error rate requires 10 hash functions and 14.4  bits per item.
* **0.01%** error rate requires 14 hash functions and 20.16 bits per item.


### Parameters:

* **key**: The key under which the filter is to be found
* **error_rate**: The desired probability for false positives. This should
    be a decimal value between 0 and 1. For example, for a desired false
    positive rate of 0.1% (1 in 1000), error_rate should be set to 0.001.
* **capacity**: The number of entries intended to be added to the filter.
    Performance will begin to degrade after adding more items than this
    number. The actual degradation will depend on how far the limit has
    been exceeded. Performance will degrade linearly as the number of entries
    grow exponentially.

Optional parameters:

* **NONSCALING**: Prevents the filter from creating additional sub-filters if
    initial capacity is reached. Non-scaling filters requires slightly less
    memory than their scaling counterparts.
* **expansion**: When `capacity` is reached, an additional sub-filter is
    created. The size of the new sub-filter is the size of the last sub-filter
    multiplied by `expansion`. If the number of elements to be stored in the
    filter is unknown, `expansion` of 2 or more is recommended to reduce the
    number of sub-filters, else, `expansion` of 1 is recommended to reduce
    memory consumption. Default expansion value is 2.

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

O(hash), the number of `hash` functions used for the latest sub-filter.

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

O(hash), the number of `hash` functions used for the latest sub-filter.

### Returns

An array of booleans (integers). Each element is either true or false depending
on whether the corresponding input element was newly added to the filter or may
have previously existed.

## BF.INSERT

```
BF.INSERT {key} [CAPACITY {cap}] [ERROR {error}] [EXPANSION {expansion}] [NOCREATE]
[NONSCALING] ITEMS {item...}
```

### Description

BF.INSERT is a sugarcoated combination of BF.RESERVE and BF.ADD. It will create a
new filter if `key` does not exist using the relevant arguments (see BF.RESERVE).
Next, all `ITEMS` are inserted.

### Parameters

* **key**: The name of the filter
* **ITEMS**: Indicates the beginning of the items to be added to the filter. This
    parameter must be specified.

Optional parameters:

* **NOCREATE**: If specified, indicates that the filter should not be created if
    it does not already exist. If the filter does not yet exist, an error is
    returned rather than creating it automatically. This may be used where a strict
    separation between filter creation and filter addition is desired. It is an
    error to specify `NOCREATE` together with either `CAPACITY` or `ERROR`.
* **capacity**: If specified, should be followed by the desired `capacity` for the
    filter to be created. This parameter is ignored if the filter already exists.
    If the filter is automatically created and this parameter is absent, then the
    default `capacity` (specified at the module-level) is used. See `BF.RESERVE`
    for more information on the impacts of this value.
* **error**: If specified, should be followed by the the `error` ratio of the newly
    created filter if it does not yet exist. If the filter is automatically
    created and `error` is not specified then the default module-level error
    rate is used. See `BF.RESERVE` for more information on the format of this
    value.
* **NONSCALING**: Prevents the filter from creating additional sub-filters if
    initial capacity is reached. Non-scaling filters requires slightly less
    memory than their scaling counterparts.
* **expansion**: When `capacity` is reached, an additional sub-filter is
    created. The size of the new sub-filter is the size of the last sub-filter
    multiplied by `expansion`. If the number of elements to be stored in the
    filter is unknown, `expansion` of 2 or more is recommended to reduce the
    number of sub-filters, else, `expansion` of 1 is recommended to reduce
    memory consumption. Default expansion value is 2.

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

O(hash), the number of `hash` functions used for the latest sub-filter.

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

O(hash * sub-filters), the number of `hash` functions multiplied by the number
of `sub-filters`.
On average, a sub-filter returns FALSE after 2 bits are tested. Therefore the
average complexity for a FALSE reply is `2 * sub-filters` while for a TRUE
reply, the complexity is `2 * 1/2 * sub-filters + hash`.

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

O(hash * sub-filters), the number of `hash` functions multiplied by the number
of `sub-filters`.
On average, a sub-filter returns FALSE after 2 bits are tested. Therefore the
average complexity for a FALSE reply is `2 * sub-filters` while for a TRUE
reply, the complexity is `2 * 1/2 * sub-filters + hash`.

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
