Deletes an item once from the filter.

If the item exists only once, it will be removed from the filter. If the item was added multiple times, it will still be present.

<note><b>Note:</b>

- Deleting an item that are not in the filter may delete a different item, resulting in false negatives.
</note>

## Required arguments

<details open><summary><code>key</code></summary>

is key name for a cuckoo filter.
</details>

<details open><summary><code>item</code></summary>

is an item to delete.
</details>

## Complexity

O(n), where n is the number of `sub-filters`. Both alternative locations are
checked on all `sub-filters`.

## Return value

Either

- @integer-reply - where "1" means that the item has been deleted, and "0" means that such item was not found in the filter
- @error-reply on error (invalid arguments, wrong key type, etc.)

## Examples

{{< highlight bash >}}
redis> CF.INSERT cf ITEMS item1 item2 item2
1) (integer) 1
2) (integer) 1
3) (integer) 1
redis> CF.DEL cf item1
(integer) 1
redis> CF.DEL cf item1
(integer) 0
redis> CF.DEL cf item2
(integer) 1
redis> CF.DEL cf item2
(integer) 1
redis> CF.DEL cf item2
(integer) 0
{{< / highlight >}}
