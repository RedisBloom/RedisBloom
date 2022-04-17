Determines if one or more items may exist in the filter or not.

### Parameters

* **key**: The name of the filter
* **items**: One or more items to check

@return

@array-reply of @integer-reply - for each item where "1" value means the
corresponding item may exist in the filter, and a "0" value means it does not
exist in the filter.

@examples

```
redis> BF.MEXISTS bf item1 item_new
1) (integer) 1
2) (integer) 0
```