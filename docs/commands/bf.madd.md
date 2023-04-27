Adds one or more items to the Bloom Filter.

This command is similar to `BF.ADD`, except that more than one item can be added.

## Required arguments

<details open><summary><code>key</code></summary>

is key name for a Bloom filter to add the items to.

If `key` does not exist - a new Bloom filter is created with default error rate, capacity, and expansion (see `BF.RESERVE`).
</details>

<details open><summary><code>item...</code></summary>

One or more items to add.
</details>

## Return value

Either

- @array-reply where each element is either
  - @integer-reply - where "1" means that the item has been added successfully, and "0" means that such item was already added to the filter (which could be wrong)
  - @error-reply when the item cannot be added because the filter is full
- @error-reply (invalid arguments, wrong key type, etc.)

## Examples

{{< highlight bash >}}
redis> BF.MADD bf item1 item2 item2
1) (integer) 1
2) (integer) 1
3) (integer) 0
{{< / highlight >}}
