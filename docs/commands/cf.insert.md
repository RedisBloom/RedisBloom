Adds one or more items to a cuckoo filter, allowing the filter to be created with a custom capacity if it does not exist yet.

This command is similar to `CF.ADD`, except that more than one item can be added and capacity can be specified.

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

- @array-reply of @integer-reply - where "1" means that the item has been successfully added to the filter, and "-1" means that the item was not added because the filter is full.
- @error-reply on error (invalid arguments, wrong key type, and so on) and also when `NOCREATE` is specified and `key` does not exist.

## Examples

{{< highlight bash >}}
redis> CF.INSERT cf CAPACITY 1000 ITEMS item1 item2 
1) (integer) 1
2) (integer) 1
{{< / highlight >}}

{{< highlight bash >}}
redis> CF.INSERT cf1 CAPACITY 1000 NOCREATE ITEMS item1 item2 
(error) ERR not found
{{< / highlight >}}

{{< highlight bash >}}
redis> CF.RESERVE cf2 2 BUCKETSIZE 1 EXPANSION 0
OK
redis> CF.INSERT cf2 ITEMS 1 1 1 1
1) (integer) 1
2) (integer) 1
3) (integer) -1
4) (integer) -1
{{< / highlight >}}
