# RedisBloom Bloom Filter Command Documentation

Based on [Age-Partitioned Bloom Filters](http://arxiv.org/pdf/2001.03147)
    by Paulo Almeida, Carlos Baquero and Ariel Shtul.

## APBF.RESERVE

### Format:

```
APBF.RESERVE {key} [COUNT/TIME] {error_rate} {capacity} [{time_span}]
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
* **time_span**: Time until an element expires.

### Complexity

O(1)

### Returns

OK on success, error otherwise.

```sql
127.0.0.1:6379> APBF.RESERVE apbfc COUNT 3 1000
OK
127.0.0.1:6379> APBF.RESERVE apbft TIME 4 5000 10
OK
```

## APBF.ADD

### Format

```
APBF.ADD {key} {item} [item ...]
```

### Description

Adds items to the Age-Partitioned Bloom-Filter, creating the filter if it
does not yet exist.

### Parameters

* **key**: The name of the filter
* **item**: The item to add

### Complexity

O(k), where k is the number of `hash` functions used by the last sub-filter.

### Returns

"1" if the item was newly inserted, or "0" if it may have existed previously.

## APBF.EXISTS

### Format

```
APBF.EXISTS {key} {item} [item ...]
```

### Description

Determines whether items exist in the Bloom Filter or not.

### Parameters

* **key**: The name of the filter
* **item**: The item to check for

### Complexity

O(s), where s is the number of `slices` in the APBF.

### Returns

"0" if the item certainly does not exist, "1" if the item may exist.

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
