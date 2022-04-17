Adds an item to the cuckoo filter, creating the filter if it does not exist.

Cuckoo filters can contain the same item multiple times, and consider each insert
as separate. You can use `CF.ADDNX` to only add the item if it does not
exist yet. Keep in mind that deleting an element inserted using `CF.ADDNX` may
cause false-negative errors.

### Parameters

* **key**: The name of the filter
* **item**: The item to add

### Complexity

O(n + i), where n is the number of `sub-filters` and i is `maxIterations`.
Adding items requires up to 2 memory accesses per `sub-filter`.
But as the filter fills up, both locations for an item might be full. The filter
attempts to `Cuckoo` swap items up to `maxIterations` times.

@return

@integer-reply - "1" if executed correctly, or @error-reply otherwise.

```
redis> CF.ADD cf item
(integer) 1
```