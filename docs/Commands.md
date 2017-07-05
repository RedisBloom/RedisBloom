# ReBloom Full Command Documentation

## BF.RESERVE

### Format:

```
BF.RESERVE {key} {error_rate} {size}
```

### Description:

Creates an empty Bloom Filter with a given error ratio and initial capacity.
This command is useful if you intend to add many items to a Bloom Filter, 
otherwise you can just use `BF.ADD` to add items. It will also create one for
you.

The initial size and error rate will dictate the performance and memory usage
of the filter. In general, the higher the lower the error ratio (i.e. the lower
the tolerance for false positives) the greater the space consumption per
filter entry.

### Parameters:

* **key**: The key under which the filter is to be found
* **error_rate**: The percentage of expected false positives to be tolerated.
    The greater this number, the greater the memory consumption per item
    and the more CPU usage per operation.
* **size**: The number of entries you intend to to add to the filter.
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
BF.ADD {key} {item}
```

### Description

Adds an item to the Bloom Filter, creating the filter if it does not yet exist.

### Parameters

* **key**: The name of the filter
* **item**: The item to add

### Complexity

O(log N).

### Returns

1 if the item was newly inserted, or 0 if it may have existed previously.


## BF.MADD

### Format

```
{key} {item} [item...]
```

### Description

Adds one or more items to the Bloom Filter, creating the filter if it does not yet exist.
This command operates identally to `BF.ADD` except it allows multiple inputs and returns
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

Check if an item may exists, or certainly does not exist in the Bloom Filter.

### Parameters

* **key**: the name of the filter
* **item**: the item to check for

### Complexity

O(log N).

### Returns

0 If the item certainly does not exist, 1 if the item may exist.


## BF.MEXISTS

### Format

```
BF.MEXISTS {key} {item} [item...]
```

### Description

Check if one or more items exist, or certainly do not exist in the filter

### Parameters

* **key**: name of the filter
* **items**: one or more items to check

### Complexity

O(log N).

### Returns

An array of boolean values (actually integers). A true value means the
corresponding item may exist in the filter, while a false value means it does not.