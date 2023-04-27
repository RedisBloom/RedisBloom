Adds an item to the cuckoo filter, creating the filter if it does not exist.

Cuckoo filters can contain the same item multiple times, and consider each insert as separate.
You can use `CF.ADDNX` to only insert an item if it does not exist yet.

## Required arguments

<details open><summary><code>key</code></summary>

is key name for a cuckoo filter to insert items to.

If `key` does not exist - a new cuckoo filter is created.
</details>

<details open><summary><code>item</code></summary>

is an item to insert.
</details>

## Return value

@integer-reply - "1" means the item has been inserted to the filter.

@error-reply on error (invalid arguments, wrong key type, etc.) and also when the filter is full.

## Complexity

O(n + i), where n is the number of `sub-filters` and i is `maxIterations`.
Inserting items requires up to 2 memory accesses per `sub-filter`.
But as the filter fills up, both locations for an item might be full. The filter
attempts to `Cuckoo` swap items up to `maxIterations` times.

## Examples

{{< highlight bash >}}
redis> CF.ADD cf item
(integer) 1
{{< / highlight >}}
