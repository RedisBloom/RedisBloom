Check if one or more `items` exists in a Cuckoo Filter `key`

### Parameters

* **key**: The name of the filter
* **items**: The item to check for

@return

@array-reply of @integer-reply - for each item where "1" value means the
corresponding item may exist in the filter, and a "0" value means it does not
exist in the filter.

@examples

```
redis> CF.MEXISTS cf item1 item_new
1) (integer) 1
2) (integer) 0
```