Adds one or more items to the Bloom Filter and creates the filter if it does not exist yet.

This command operates identically to `BF.ADD` except that it allows multiple inputs and returns multiple values.

## Required arguments

<details open><summary><code>key</code></summary>

is key name for a Bloom filter to insert items to.

If `key` does not exist - a new Bloom filter is created.
</details>

<details open><summary><code>item...</code></summary>

One or more items to insert.
</details>

## Return value

Either

- @array-reply where each element is either
  - @integer-reply - where "0" means that an item with such fingerprint already exists in the filter, and "1" means that the item has been successfully inserted to the filter
  - @error-reply when the item cannot be inserted because the filter is full
- @error-reply (e.g., on wrong number of arguments, wrong key type)

## Examples

{{< highlight bash >}}
redis> BF.MADD bf item1 item2 item2
1) (integer) 1
2) (integer) 1
3) (integer) 0
{{< / highlight >}}
