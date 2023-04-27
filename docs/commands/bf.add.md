Adds an item to a Bloom filter if the item did not exist previously; creating the filter if it does not exist.

## Required arguments

<details open><summary><code>key</code></summary>

is key name for a Bloom filter to insert items to.

If `key` does not exist - a new Bloom filter is created.
</details>

<details open><summary><code>item</code></summary>

is an item to insert.
</details>

## Return value

@integer-reply - where "0" means that an item with such fingerprint already exists in the filter, and "1" means the item has been inserted to the filter.

@error-reply on error (invalid arguments, wrong key type, etc.) and also when the filter is full.

## Examples

{{< highlight bash >}}
redis> BF.ADD bf item1
(integer) 1
redis> BF.ADD bf item1
(integer) 0
{{< / highlight >}}
