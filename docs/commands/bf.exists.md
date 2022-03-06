Determines whether an item may exist in the Bloom Filter or not.

### Parameters

* **key**: The name of the filter
* **item**: The item to check for

@return

@integer-reply - where "1" value means the item may exist in the filter,
and a "0" value means it does not exist in the filter.

@examples

```
redis> BF.EXISTS bf item1
(integer) 1
redis> BF.EXISTS bf item_new
(integer) 0
```