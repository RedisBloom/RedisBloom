Adds an item to a cuckoo filter if the item did not exist previously; creating the filter if it does not exist.

This command is equivalent to a `CF.EXISTS` + `CF.ADD` command. It does not insert an item into the filter if its fingerprint already exists and therefore better utilizes the available capacity.

<note><b>Notes:</b>

- This command is slower than `CF.ADD` because it first checks whether the item exists.
- Since `CF.EXISTS` can result in false positive, `CF.ADDNX` may not insert an item because it is supposedly already exist, which may be wrong.

</note>

## Required arguments

<details open><summary><code>key</code></summary>

is key name for a cuckoo filter to insert items to.

If `key` does not exist - a new cuckoo filter is created.
</details>

<details open><summary><code>item</code></summary>

is an item to insert.
</details>

## Return value

@integer-reply - where "0" means that an item with such fingerprint already exist in the filter, "1" means the item has been inserted to the filter.

@error-reply on error (invalid arguments, wrong key type, etc.) and also when the filter is full.

## Examples

{{< highlight bash >}}
redis> CF.ADDNX cf item
(integer) 1
redis> CF.ADDNX cf item
(integer) 0
{{< / highlight >}}
