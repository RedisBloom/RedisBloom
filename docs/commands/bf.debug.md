BF.DEBUG return more information about the bloom filter `key`.

### Parameters

* **key**: The name of the filter

@return

* **size**: Total number of items in all filters.
* **bytes**: The bytes size of every filter.
* **bits**: The bits size of every filter.
* **hashes**: The number of hash functions.
* **hashwidth**: 64-bit hash or 32-bit hash.
* **capacity**: The entries size of every filter.
* **size**: The number of items in the filter.
* **ratio**: The error ratio of filter.

@examples

```
redis> BF.DEBUG newFilter1
(error) ERR not found
```

```
redis> BF.DEBUG newFilter
1) "size:497"
2) "bytes:144 bits:1152 hashes:8 hashwidth:64 capacity:100 size:100 ratio:0.005"
3) "bytes:312 bits:2496 hashes:9 hashwidth:64 capacity:200 size:200 ratio:0.0025"
4) "bytes:696 bits:5568 hashes:10 hashwidth:64 capacity:400 size:197 ratio:0.00125"
```
