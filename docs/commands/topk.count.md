Returns count for an item. 
Multiple items can be requested at once.
Please note this number will never be higher than the real count and likely to be lower.

### Parameters

* **key**: Name of sketch where item is counted.
* **item**: Item/s to be counted.

@return

@array-reply of @integer-reply - count for responding item.

@examples

```
redis> TOPK.COUNT topk foo 42 nonexist
1) (integer) 3
2) (integer) 1
3) (integer) 0
```
