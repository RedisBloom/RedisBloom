Determines whether a given item was added to a Bloom filter.

This command is similar to `BF.MEXISTS`, except that only one item can be checked.

## Required arguments

<details open><summary><code>key</code></summary>

is key name for a Bloom filter.

</details>

<details open><summary><code>item</code></summary>

is an item to check.
</details>

## Return value

Either

- @integer-reply, where `1` means that, with high probability, `item` was already added to the filter, and `0` means that `key` does not exist or that `item` had not been added to the filter.
- @error-reply on error (invalid arguments, wrong key type, etc.)

## Examples

{{< highlight bash >}}
redis> BF.ADD bf item1
(integer) 1
redis> BF.EXISTS bf item1
(integer) 1
redis> BF.EXISTS bf item2
(integer) 0
{{< / highlight >}}
