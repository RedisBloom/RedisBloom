Adds one or more items to a cuckoo filter if they did not exist previously, allowing the filter to be created with a custom capacity if it does not exist yet.

This command is equivalent to a `CF.EXISTS` + `CF.ADD` command. It does not add an item into the filter if its fingerprint already exists and therefore better utilizes the available capacity. 

This command is equivalent to a `CF.ADDNX`, except that more than one item can be added and capacity can be specified.

<note><b>Notes:</b>

- This command is slower than `CF.INSERT` because it first checks whether each item exists.
- Since `CF.EXISTS` can result in false positive, `CF.INSERTNX` may not add an item because it is supposedly already exist, which may be wrong.
    
</note>

## Required arguments

<details open><summary><code>key</code></summary>

is key name for a cuckoo filter to add items to.

If `key` does not exist - a new cuckoo filter is created.
</details>

<details open><summary><code>ITEMS item...</code></summary>

One or more items to add.
</details>

## Optional arguments

<details open><summary><code>CAPACITY capacity</code></summary>
    
Specifies the desired capacity of the new filter, if this filter does not exist yet.
    
If the filter already exists, then this parameter is ignored.
    
If the filter does not exist yet and this parameter is *not* specified, then the filter is created with the module-level default capacity which is 1024.

See `CF.RESERVE` for more information on cuckoo filter capacities.
</details>
    
<details open><summary><code>NOCREATE</code></summary>
  
If specified, prevents automatic filter creation if the filter does not exist (Instead, an error is returned).
    
This option is mutually exclusive with `CAPACITY`.
</details>

## Return value

Either

- @array-reply of @integer-reply - where "0" means that an item with such fingerprint already exist in the filter, "1" means that the item has been successfully added to the filter, and "-1" means that the item was not added because the filter is full.
- @error-reply on error (invalid arguments, wrong key type, etc.)

### Complexity

O(n + i), where n is the number of `sub-filters` and i is `maxIterations`.
Adding items requires up to 2 memory accesses per `sub-filter`.
But as the filter fills up, both locations for an item might be full. The filter attempts to `Cuckoo` swap items up to `maxIterations` times.

## Examples

{{< highlight bash >}}
redis> CF.INSERTNX cf CAPACITY 1000 ITEMS item1 item2 
1) (integer) 1
2) (integer) 1
{{< / highlight >}}

{{< highlight bash >}}
redis> CF.INSERTNX cf CAPACITY 1000 ITEMS item1 item2 item3
1) (integer) 0
2) (integer) 0
3) (integer) 1
{{< / highlight >}}

{{< highlight bash >}}
redis> CF.INSERTNX cf_new CAPACITY 1000 NOCREATE ITEMS item1 item2 
(error) ERR not found
{{< / highlight >}}
