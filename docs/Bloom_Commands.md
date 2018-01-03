# ReBloom Bloom Filter Command Documentation

## BF.RESERVE

### Format:

```
BF.RESERVE {key} {error_rate} {size}
```

### Description:

Creates an empty Bloom Filter with a given desired error ratio and initial capacity.
This command is useful if you intend to add many items to a Bloom Filter, 
otherwise you can just use `BF.ADD` to add items. It will also create a Bloom Filter for
you if one doesn't already exist.

The initial size and error rate will dictate the performance and memory usage
of the filter. In general, the smaller the error rate (i.e. the lower
the tolerance for false positives) the greater the space consumption per
filter entry.

### Parameters:

* **key**: The key under which the filter is to be found
* **error_rate**: The desired probability for false positives. This should
    be a decimal value between 0 and 1. For example, for a desired false
    positive rate of 0.1% (1 in 1000), error_rate should be set to 0.001.
    The closer this number is to zero, the greater the memory consumption per
    item and the more CPU usage per operation.
* **size**: The number of entries you intend to add to the filter.
    Performance will begin to degrade after adding more items than this
    number. The actual degradation will depend on how far the limit has
    been exceeded. Performance will degrade linearly as the number of entries
    grow exponentially.

### Complexity

O(1)

### Returns

OK on success, error otherwise.

## BF.ADD

### Format

```
BF.ADD {key} {item} [RESERVE {error_rate} {size}]
```

### Description

Adds an item to the Bloom Filter, creating the filter if it does not yet exist.

If the `RESERVE` keyword is used, and the filter does not yet exist, it is as if
`BF.RESERVE` was invoked with the passed parameters. This allows the in-place
creation of filters to use custom size/error requirements and override the defaults.

### Parameters

* **key**: The name of the filter
* **item**: The item to add

### Complexity

O(log N).

### Returns

"1" if the item was newly inserted, or "0" if it may have existed previously.


## BF.MADD

### Format

```
{key} {item} [item...]
```

### Description

Adds one or more items to the Bloom Filter, creating the filter if it does not yet exist.
This command operates identically to `BF.ADD` except it allows multiple inputs and returns
multiple values.

### Parameters

* **key**: The name of the filter
* **items**: One or more items to add

### Complexity

O(log N).

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

O(log N).

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

O(log N).

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

O(log N)

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

O(log N)

### Returns

`OK` on success, or an error on failure.