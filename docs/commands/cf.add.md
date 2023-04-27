Adds an item to the cuckoo filter, creating the filter if it does not exist.

Cuckoo filters can contain the same item multiple times, and consider each insert
as separate. You can use `CF.ADDNX` to only add the item if it does not
exist yet. Keep in mind that deleting an element inserted using `CF.ADDNX` may
cause false-negative errors.

## Required arguments

* **key**: The name of the filter
* **item**: The item to add

## Return value

@integer-reply - "1" means the item has been added to the filter.

@error-reply on error (invalid arguments, wrong key type, etc.) and also when the filter is full.

## Complexity

O(n + i), where n is the number of `sub-filters` and i is `maxIterations`.
Adding items requires up to 2 memory accesses per `sub-filter`.
But as the filter fills up, both locations for an item might be full. The filter
attempts to `Cuckoo` swap items up to `maxIterations` times.

## Examples

{{< highlight bash >}}
redis> CF.ADD cf item
(integer) 1
{{< / highlight >}}
