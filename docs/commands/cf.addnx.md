Adds an item to a cuckoo filter if the item did not exist previously.
See documentation on `CF.ADD` for more information on this command.

This command is equivalent to a `CF.EXISTS` + `CF.ADD` command. It does not
insert an element into the filter if its fingerprint already exists in order to
use the available capacity more efficiently. However, deleting
elements can introduce **false negative** error rate!

Note that this command is slower than `CF.ADD` because it first checks whether the
item exists.

## Required arguments

<details open><summary><code>key</code></summary>

is key name for a cuckoo filter to insert items to.

If `key` does not exist - a new cuckoo filter is created.
</details>

<details open><summary><code>item</code></summary>

is an item to insert.
</details>

## Return value

@integer-reply - where "0" means that an element with such fingerprint already exist in the filter, "1" means the item has been inserted to the filter.

@error-reply on error (invalid arguments, wrong key type, etc.) and also when the filter is full.

## Examples

{{< highlight bash >}}
redis> CF.ADDNX cf item1
(integer) 0
redis> CF.ADDNX cf item_new
(integer) 1
{{< / highlight >}}
