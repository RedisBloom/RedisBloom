Determines whether one or more items were added to a Bloom filter.

This command is similar to `BF.EXISTS`, except that more than one item can be checked.

## Required arguments

<details open><summary><code>key</code></summary>

is key name for a Bloom filter.

</details>

<details open><summary><code>item...</code></summary>

One or more items to check.
</details>

## Return value

Either

- @array-reply of @integer-reply - where "1" means that, with high probability, `item` was already added to the filter, and "0" means that `key` does not exist or that `item` was definitely not added to the filter.
- @error-reply on error (invalid arguments, wrong key type, etc.)

## Examples

{{< highlight bash >}}
redis> BF.MADD bf item1 item2
1) (integer) 1
2) (integer) 1
redis> BF.MEXISTS bf item1 item2 item3
1) (integer) 1
2) (integer) 1
3) (integer) 0
{{< / highlight >}}
