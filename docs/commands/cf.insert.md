Adds one or more items to a cuckoo filter, allowing the filter to be created
with a custom capacity if it does not exist yet.

These commands offers more flexibility over the `ADD` command, at
the cost of more verbosity.

### Parameters

* **key**: The name of the filter
* **capacity**: Specifies the desired capacity of the new filter, if this filter
    does not exist yet. If the filter already exists, then this parameter is
    ignored. If the filter does not exist yet and this parameter is *not*
    specified, then the filter is created with the module-level default capacity
    which is 1024. See `CF.RESERVE` for more information on cuckoo filter
    capacities.
* **NOCREATE**: If specified, prevents automatic filter creation if the filter
    does not exist. Instead, an error is returned if the filter does not
    already exist. This option is mutually exclusive with `CAPACITY`.
* **item**: One or more items to add. The `ITEMS` keyword must precede the list of items to add.

@return

@integer-reply - "1" if executed correctly, or @error-reply otherwise.

@examples

```
redis> CF.INSERT cf CAPACITY 1000 ITEMS item1 item2 
1) (integer) 1
2) (integer) 1
```

```
redis> CF.INSERT cf1 CAPACITY 1000 NOCREATE ITEMS item1 item2 
(error) ERR not found
```