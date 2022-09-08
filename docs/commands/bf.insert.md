BF.INSERT is a sugarcoated combination of BF.RESERVE and BF.ADD. It creates a
new filter if the `key` does not exist using the relevant arguments (see BF.RESERVE).
Next, all `ITEMS` are inserted.

### Parameters

* **key**: The name of the filter
* **item**: One or more items to add. The `ITEMS` keyword must precede the list of items to add.

Optional parameters:

* **NOCREATE**: (Optional) Indicates that the filter should not be created if
    it does not already exist. If the filter does not yet exist, an error is
    returned rather than creating it automatically. This may be used where a strict
    separation between filter creation and filter addition is desired. It is an
    error to specify `NOCREATE` together with either `CAPACITY` or `ERROR`.
* **capacity**: (Optional) Specifies the desired `capacity` for the
    filter to be created. This parameter is ignored if the filter already exists.
    If the filter is automatically created and this parameter is absent, then the
    module-level `capacity` is used. See `BF.RESERVE`
    for more information about the impact of this value.
* **error**: (Optional) Specifies the `error` ratio of the newly
    created filter if it does not yet exist. If the filter is automatically
    created and `error` is not specified then the module-level error
    rate is used. See `BF.RESERVE` for more information about the format of this
    value.
* **NONSCALING**: Prevents the filter from creating additional sub-filters if
    initial capacity is reached. Non-scaling filters require slightly less
    memory than their scaling counterparts. The filter returns an error
    when `capacity` is reached.
* **expansion**: When `capacity` is reached, an additional sub-filter is
    created. The size of the new sub-filter is the size of the last sub-filter
    multiplied by `expansion`. If the number of elements to be stored in the
    filter is unknown, we recommend that you use an `expansion` of 2 or more
    to reduce the number of sub-filters. Otherwise, we recommend that you use an
    `expansion` of 1 to reduce memory consumption. The default expansion value is 2.

@return

An array of booleans (integers). Each element is either true or false depending
on whether the corresponding input element was newly added to the filter or may
have previously existed.


@examples
Add three items to a filter with default parameters if the filter does not already
exist:

```
BF.INSERT filter ITEMS foo bar baz
```

Add one item to a filter with a capacity of 10000 if the filter does not
already exist:

```
BF.INSERT filter CAPACITY 10000 ITEMS hello
```

Add 2 items to a filter with an error if the filter does not already exist:

```
BF.INSERT filter NOCREATE ITEMS foo bar
```
