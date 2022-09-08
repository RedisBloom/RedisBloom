
Adds an item to the data structure. 
Multiple items can be added at once.
If an item enters the Top-K list, the item which is expelled is returned.
This allows dynamic heavy-hitter detection of items being entered or expelled from Top-K list. 

### Parameters

* **key**: Name of sketch where item is added.
* **item**: Item/s to be added.

### Return

@array-reply of @simple-string-reply - if an element was dropped from the TopK list, @nil-reply otherwise..

#### Example

```
redis> TOPK.ADD topk foo bar 42
1) (nil)
2) baz
3) (nil)
```