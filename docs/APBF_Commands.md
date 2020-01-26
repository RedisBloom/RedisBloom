# RedisBloom Bloom Filter Command Documentation

Based on [Age-Partitioned Bloom Filters](http://arxiv.org/pdf/2001.03147)
    by Paulo Almeida, Carlos Baquero and Ariel Shtul.

## APBF.RESERVE

### Format:

```
APBF.RESERVE {key} {error_rate} {capacity}
```

### Description:

Creates an empty Age-Partition Bloom-Filter with name `key` and `capacity`.
`error_rate` is precision point. ex. 3 -> 0.001 or 0.1%.
The filter is made from `k + l` slices where `k` is the number of hash
functions and `l` are additional slices which contain the "memory" of the
filter and are being retired as additional elements are being inserted into the
filter.

### Parameters:

* **key**: The key under which the filter is found
* **error_rate**: The desired probability for false positives. The rate is
    an integer from 1 to 5.
* **capacity**: The number of latest entries in the filter. Therefore, out of
    an infinite flow of elements, only `capacity` number of elements will
    return True-Positive. All other elements will return False-Positive at
    an `error_rate` percentage of the time. 

### Complexity

O(1)

### Returns

OK on success, error otherwise.

```sql
127.0.0.1:6379> APBF.RESERVE apbf_1 3 1000
OK
127.0.0.1:6379> APBF.RESERVE apbf_2 4 50000
OK
```

## BF.ADD

### Format

```
APBF.ADD {key} {item}
```

### Description

Adds an item to the Age-Partitioned Bloom-Filter, creating the filter if it
does not yet exist.

### Parameters

* **key**: The name of the filter
* **item**: The item to add

### Complexity

O(k), where k is the number of `hash` functions used by the last sub-filter.

### Returns

"1" if the item was newly inserted, or "0" if it may have existed previously.

## APBF.MADD (WIP)

### Format

```
BF.MADD {key} {item} [item...]
```

### Description

Adds one or more items to the Bloom Filter and creates the filter if it does
not exist yet.
This command operates identically to `APBF.ADD` except that it allows multiple
inputs and returns multiple values.

### Parameters

* **key**: The name of the filter
* **items**: One or more items to add

### Complexity

O(k * n), where k is the number of `hash` functions used by the last sub-filter
and m the number of items that are added.

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

* **key**: The name of the filter
* **item**: The item to check for

### Complexity

O(s), where s is the number of `slices` in the APBF.

### Returns

"0" if the item certainly does not exist, "1" if the item may exist.


## BF.MEXISTS (WIP)

### Format

```
BF.MEXISTS {key} {item} [item...]
```

### Description

Determines if one or more items may exist in the filter or not.

### Parameters

* **key**: The name of the filter
* **items**: One or more items to check

### Complexity

O(m * s), where m is the number of added elements and s is the number of
`slices` in the APBF.

### Returns

An array of boolean values (actually integers). A true value means the
corresponding item may exist in the filter, and a false value means it does not
exist in the filter.

## APBF.INFO

### Format

```
APBF.INFO {key}
```

### Description

Return information about `key`

### Parameters

* **key**: Name of the key to restore

### Complexity O

O(1)

### Returns

```sql
127.0.0.1:6379> APBF.INFO apbf
 1) Size
 2) (integer) 420608
 3) Capacity
 4) (integer) 100000
 5) Error rate
 6) (integer) 4
 7) Inserts count
 8) (integer) 0
 9) Hash functions count
10) (integer) 18
11) Periods count
12) (integer) 63
13) Slices count
14) (integer) 81
```
