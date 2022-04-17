Check if an `item` exists in a Cuckoo Filter `key`

### Parameters

* **key**: The name of the filter
* **item**: The item to check for

@return

@integer-reply - where "1" value means the item may exist in the filter,
and a "0" value means it does not exist in the filter.

@examples

```
redis> CF.EXISTS cf item1
(integer) 1
```
```
redis> CF.EXISTS cf item_new
(integer) 0
```