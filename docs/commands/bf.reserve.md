Creates an empty Bloom Filter with a single sub-filter for the initial capacity
requested and with an upper bound `error_rate`. By default, the filter
auto-scales by creating additional sub-filters when `capacity` is reached. The
new sub-filter is created with size of the previous sub-filter multiplied by
`expansion`.

Though the filter can scale up by creating sub-filters, it is recommended to
reserve the estimated required `capacity` since maintaining and querying
sub-filters requires additional memory (each sub-filter uses an extra bits and
hash function) and consume  further CPU time than an equivalent filter that had
the right capacity at creation time.

The number of hash functions is `-log(error)/ln(2)^2`.
The number of bits per item is `-log(error)/ln(2)` ≈ 1.44.

* **1%**    error rate requires 7  hash functions and 10.08 bits per item.
* **0.1%**  error rate requires 10 hash functions and 14.4  bits per item.
* **0.01%** error rate requires 14 hash functions and 20.16 bits per item.


### Parameters:

* **key**: The key under which the filter is found
* **error_rate**: The desired probability for false positives. The rate is
    a decimal value between 0 and 1. For example, for a desired false
    positive rate of 0.1% (1 in 1000), error_rate should be set to 0.001.
* **capacity**: The number of entries intended to be added to the filter.
    If your filter allows scaling, performance will begin to degrade after
    adding more items than this number. The actual degradation depends on
    how far the limit has been exceeded. Performance degrades linearly with
    the number of `sub-filters`.

Optional parameters:

* **NONSCALING**: Prevents the filter from creating additional sub-filters if
    initial capacity is reached. Non-scaling filters requires slightly less
    memory than their scaling counterparts. The filter returns an error
    when `capacity` is reached.
* **EXPANSION**: When `capacity` is reached, an additional sub-filter is
    created. The size of the new sub-filter is the size of the last sub-filter
    multiplied by `expansion`. If the number of elements to be stored in the
    filter is unknown, we recommend that you use an `expansion` of 2 or more
    to reduce the number of sub-filters. Otherwise, we recommend that you use an
    `expansion` of 1 to reduce memory consumption. The default expansion value is 2.

@return

@simple-string-reply - `OK` if executed correctly, or @error-reply otherwise.

@examples

```
redis> BF.RESERVE bf 0.01 1000
OK
```

```
redis> BF.RESERVE bf 0.01 1000
(error) ERR item exists
```

```
redis> BF.RESERVE bf_exp 1000 EXPANSION 2
OK
```

```
redis> BF.RESERVE bf_exp 1000 NONSCALING
OK
```