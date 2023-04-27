Adds an item to a Bloom filter.

If the Bloom filter does not exist - it is created with default error rate, capacity, and expansion (see `BF.RESERVE`).

## Required arguments

<details open><summary><code>key</code></summary>

is key name for a Bloom filter to add the item to.

If `key` does not exist - a new Bloom filter is created.
</details>

<details open><summary><code>item</code></summary>

is an item to add.
</details>

## Return value

Either

- @integer-reply - where "1" means that the item has been added successfully, and "0" means that such item was already added to the filter (which could be wrong).
- @error-reply on error (invalid arguments, wrong key type, etc.) and also when the filter is full.

## Examples

{{< highlight bash >}}
redis> BF.ADD bf item1
(integer) 1
redis> BF.ADD bf item1
(integer) 0
{{< / highlight >}}
