Returns the cardinality of a Bloom filter - number of items that were added to a Bloom filter and detected as unique (items that caused at least one bit to be set in at least one sub-filter)

## **Note**: this command will be available on the next release.

### Parameters

* **key**: The name of the filter

@return

* @integer-reply - the number of items that were added to this Bloom filter and detected as unique (items that caused at least one bit to be set in at least one sub-filter).
* 0 when `key` does not exist.
* Error when `key` is of a type other than Bloom filter.

Note: when `key` exists - return the same value as `BF.INFO key ITEMS`.

@examples

```
redis> BF.ADD bf1 item_foo
(integer) 1
redis> BF.CARD bf1
(integer) 1
redis> BF.CARD bf_new
(integer) 0
```
