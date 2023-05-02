Determines whether a given item was added to a cuckoo filter.

This command is similar to `CF.MEXISTS`, except that only one item can be checked.

## Required arguments

<details open><summary><code>key</code></summary>

is key name for a cuckoo filter.

</details>

<details open><summary><code>item</code></summary>

is an item to check.
</details>

## Return value

Either

- @integer-reply - where "1" means that, with high probability, `item` was already added to the filter, and "0" means that `key` does not exist or that `item` had not added to the filter. See note in `CF.DEL`.
- @error-reply on error (invalid arguments, wrong key type, etc.)

## Examples

{{< highlight bash >}}
redis> CF.ADD cf item1
(integer) 1
redis> CF.EXISTS cf item1
(integer) 1
redis> CF.EXISTS cf item2
(integer) 0
{{< / highlight >}}
