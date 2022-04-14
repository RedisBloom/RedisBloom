Increase the score of an item in the data structure by increment. 
Multiple items' score can be increased at once.
If an item enters the Top-K list, the item which is expelled is returned.

### Parameters

* **key**: Name of sketch where item is added.
* **item**: Item/s to be added.
* **increment**: increment to current item score.

@return

@array-reply of @simple-string-reply - if an element was dropped from the TopK list, @nil-reply otherwise..

@example

```
redis> TOPK.INCRBY topk foo 3 bar 2 42 30
1) (nil)
2) (nil)
3) foo
```