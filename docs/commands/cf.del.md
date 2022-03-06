
## CF.DEL

```
CF.DEL {key} {item}
```
### Description

Deletes an item once from the filter. If the item exists only once, it will be
removed from the filter. If the item was added multiple times, it will still be
present.

!!! danger ""
    Deleting elements that are not in the filter may delete a different item,
    resulting in false negatives!

### Parameters

* **key**: The name of the filter
* **item**: The item to delete from the filter

### Complexity

O(n), where n is the number of `sub-filters`. Both alternative locations are
checked on all `sub-filters`.

@return

@integer-reply - where "1" means the item has been deleted from the filter, and "0" mean, the item was not found.

@examples

```
redis> CF.DEL cf item1
(integer) 1
```
```
redis> CF.DEL cf item_new
(integer) 0
```
```
redis> CF.DEL cf1 item_new
(error) Not found
```