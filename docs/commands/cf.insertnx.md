
## CF.INSERT

## CF.INSERTNX

Note: `CF.INSERTNX` is an advanced command that can have unintended impact if used
incorrectly.

```
CF.INSERT {key} [CAPACITY {capacity}] [NOCREATE] ITEMS {item ...}
CF.INSERTNX {key} [CAPACITY {capacity}] [NOCREATE] ITEMS {item ...}
```

### Description

Adds one or more items to a cuckoo filter, allowing the filter to be created
with a custom capacity if it does not exist yet.

This command is equivalent to a `CF.EXISTS` + `CF.ADD` command. It does not
insert an element into the filter if its fingerprint already exists and
therefore better utilizes the available capacity. However, if you delete
elements it might introduce **false negative** error rate!

These commands offers more flexibility over the `ADD` and `ADDNX` commands, at
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

### Complexity

O(n + i), where n is the number of `sub-filters` and i is `maxIterations`.
Adding items requires up to 2 memory accesses per `sub-filter`.
But as the filter fills up, both locations for an item might be full. The filter
attempts to `Cuckoo` swap items up to `maxIterations` times.

### Returns

An array of booleans (as integers) corresponding to the items specified. Possible
values for each element are:

* `> 0` if the item was successfully inserted
* `0` if the item already existed *and* `INSERTNX` is used.
* `<0` if an error occurred

Note that for `CF.INSERT`, the return value is always be an array of `>0` values,
unless an error occurs.


@return

@array-reply of @integer-reply - where "1" means the item has been added to the filter,
and "0" mean, the item already existed.
@error-reply when filter parameters are erroneous

@examples

```
redis> CF.INSERTNX cf CAPACITY 1000 ITEMS item1 item2 
1) (integer) 1
2) (integer) 1
```

```
redis> CF.INSERTNX cf CAPACITY 1000 ITEMS item1 item2 item3
1) (integer) 0
2) (integer) 0
3) (integer) 1
```

```
redis> CF.INSERTNX cf_new CAPACITY 1000 NOCREATE ITEMS item1 item2 
(error) ERR not found
```