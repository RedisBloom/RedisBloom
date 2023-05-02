Returns an estimation of the number of times a given item was added to a cuckoo filter.

When the result is `0`, the item was not added to the filter. When the result is positive, it can be an overestimation, but not an underestimation (see note in `CF.DEL`).

If you just want to check that a given item was added to a cuckoo filter, use `CF.EXISTS`.

## Required arguments

<details open><summary><code>key</code></summary>

is key name for a cuckoo filter.

</details>

<details open><summary><code>item</code></summary>

is an item to check.
</details>

## Return value

Returns one of these replies:

- @integer-reply, where a positive value is an estimation of the number of times `item` was added to the filter. `0` means that `key` does not exist or that `item` had not been added to the filter.
- @error-reply on error (invalid arguments, wrong key type, etc.)

## Examples

{{< highlight bash >}}
redis> CF.INSERT cf ITEMS item1 item2 item2
1) (integer) 1
2) (integer) 1
3) (integer) 1
redis> CF.COUNT cf item1
(integer) 1
redis> CF.COUNT cf item2
(integer) 2
{{< / highlight >}}
