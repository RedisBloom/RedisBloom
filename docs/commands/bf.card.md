Returns the cardinality (number of items inserted) of the bloom filter stored at key.

### Parameters

* **key**: The name of the filter

@return

@integer-reply - the cardinality (number of items inserted) of the bloom filter, or 0 if key does not exist.

@examples

```
redis> BF.ADD item1 item_foo
(integer) 1
redis9> BF.CARD item1
(integer) 1
redis9> BF.CARD item_new
(integer) 0
```