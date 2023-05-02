Adds an item to the cuckoo filter.

Cuckoo filters can contain the same item multiple times, and consider each addition as separate.
Use `CF.ADDNX` to add an item only if it does not exist.

## Required arguments

<details open><summary><code>key</code></summary>

is key name for a cuckoo filter to add the item to.

If `key` does not exist - a new cuckoo filter is created.
</details>

<details open><summary><code>item</code></summary>

is an item to add.
</details>

## Return value

Returns one of these replies:

- @integer-reply - where "1" means that the item has been added successfully
- @error-reply on error (invalid arguments, wrong key type, etc.) and also when the filter is full

## Complexity

O(n + i), where n is the number of `sub-filters` and i is `maxIterations`.
Adding items requires up to 2 memory accesses per `sub-filter`.
But as the filter fills up, both locations for an item might be full.
The filter attempts to `Cuckoo` swap items up to `maxIterations` times.

## Examples

{{< highlight bash >}}
redis> CF.ADD cf item1
(integer) 1
redis> CF.ADD cf item1
(integer) 1
{{< / highlight >}}
