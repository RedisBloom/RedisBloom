Checks whether an item is one of Top-K items.
Multiple items can be checked at once.

### Parameters

* **key**: Name of sketch where item is queried.
* **item**: Item/s to be queried.

@return

@array-reply of @integer-reply - "1" if item is in Top-K, otherwise "0".

@examples

```
redis> TOPK.QUERY topk 42 nonexist
1) (integer) 1
2) (integer) 0
```