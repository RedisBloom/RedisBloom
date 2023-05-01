Creates a new Bloom filter if the `key` does not exist using the specified error rate, capacity, and expansion, then adds all specified items to the Bloom Filter.

This command is similar to `BF.MADD`, except that the error rate, capacity, and expansion can be specified. It is a sugarcoated combination of `BF.RESERVE` and `BF.MADD`.

## Required arguments

<details open><summary><code>key</code></summary>

is key name for a Bloom filter to add items to.

If `key` does not exist - a new Bloom filter is created.
</details>

<details open><summary><code>ITEMS item...</code></summary>

One or more items to add.
</details>

## Optional arguments

<details open><summary><code>NOCREATE</code></summary>

Indicates that the filter should not be created if it does not already exist.
If the filter does not yet exist, an error is returned rather than creating it automatically.
This may be used where a strict separation between filter creation and filter addition is desired.
It is an error to specify `NOCREATE` together with either `CAPACITY` or `ERROR`.
</details>

<details open><summary><code>CAPACITY capacity</code></summary>

Specifies the desired `capacity` for the filter to be created.
This parameter is ignored if the filter already exists.
If the filter is automatically created and this parameter is absent, then the module-level `capacity` is used.
See `BF.RESERVE` for more information about the impact of this value.
</details>

<details open><summary><code>ERROR error</code></summary>
    
Specifies the `error` ratio of the newly created filter if it does not yet exist.
If the filter is automatically created and `error` is not specified then the module-level error rate is used.
See `BF.RESERVE` for more information about the format of this value.
</details>

<details open><summary><code>NONSCALING</code></summary>

Prevents the filter from creating additional sub-filters if initial capacity is reached.
Non-scaling filters require slightly less memory than their scaling counterparts. The filter returns an error when `capacity` is reached.
</details>

<details open><summary><code>EXPANSION expansion</code></summary>

When `capacity` is reached, an additional sub-filter is created.
The size of the new sub-filter is the size of the last sub-filter multiplied by `expansion` - a positive integer.

If the number of elements to be stored in the filter is unknown, we recommend that you use an `expansion` of 2 or more to reduce the number of sub-filters.
Otherwise, we recommend that you use an `expansion` of 1 to reduce memory consumption. The default value is 2.
</details>

## Return value

Either

- @array-reply where each element is either
  - @integer-reply - where "1" means that the item has been added successfully, and "0" means that such item was already added to the filter (which could be wrong)
  - @error-reply when the item cannot be added because the filter is full
- @error-reply (e.g., on wrong number of arguments, wrong key type) and also when `NOCREATE` is specified and `key` does not exist.

## Examples

Add three items to a filter; create the filter with default parameters if it does not already exist:

{{< highlight bash >}}
BF.INSERT filter ITEMS foo bar baz
{{< / highlight >}}

Add one item to a filter; create the filter with a capacity of 10000 if it does not already exist:

{{< highlight bash >}}
BF.INSERT filter CAPACITY 10000 ITEMS hello
{{< / highlight >}}

Add 2 items to a filter; error if the filter does not already exist:

{{< highlight bash >}}
BF.INSERT filter NOCREATE ITEMS foo bar
{{< / highlight >}}
