Creates an empty cuckoo filter with a single sub-filter for the initial specified capacity.

Because of how cuckoo filters work, the filter is likely to declare itself full before `capacity` is reached and therefore fill rate will likely never reach 100%.
The fill rate can be improved by using a larger `bucketsize` at the cost of a higher error rate.
When the filter self-declare itself `full`, it will auto-expand by generating additional sub-filters at the cost of reduced performance and increased error rate.
The new sub-filter is created with size of the previous sub-filter multiplied by `expansion`.
Like bucket size, additional sub-filters grow the error rate linearly.

The minimal false positive error rate is 2/255 ≈ 0.78% when bucket size of 1 is used.
Larger buckets increase the error rate linearly (for example, a bucket size of 3 yields a 2.35% error rate) but improve the fill rate of the filter.

`maxiterations` dictates the number of attempts to find a slot for the incoming fingerprint.
Once the filter gets full, high `maxIterations` value will slow down insertions.

Unused capacity in prior sub-filters is automatically used when possible. 
The filter can grow up to 32 times.

## Required arguments

<details open><summary><code>key</code></summary>

is key name for the the cuckoo filter to be created.
</details>

<details open><summary><code>capacity</code></summary>

Estimated capacity for the filter. Capacity is rounded to the next `2^n` number. The filter will likely not fill up to 100% of it's capacity.
Make sure to reserve extra capacity if you want to avoid expansions.
</details>

## Optional arguments

<details open><summary><code>BUCKETSIZE bucketsize</code></summary>

Number of items in each bucket. A higher bucket size value improves the fill rate but also causes a higher error rate and slightly slower performance. The default value is 2.
</details>

<details open><summary><code>MAXITERATIONS maxiterations</code></summary>

Number of attempts to swap items between buckets before declaring filter as full and creating an additional filter. A low value is better for performance and a higher number is better for filter fill rate. The default value is 20.
</details>

<details open><summary><code>EXPANSION expansion</code></summary>

When a new filter is created, its size is the size of the current filter multiplied by `expansion` - a non-negative integer. Expansion is rounded to the next `2^n` number. The default value is 1.
</details>

## Return value

Either

- @simple-string-reply - `OK` if filter created successfully
- @error-reply on error (invalid arguments, key already exists, etc.)

## Examples

{{< highlight bash >}}
redis> CF.RESERVE cf 1000
OK

redis> CF.RESERVE cf 1000
(error) ERR item exists

redis> CF.RESERVE cf_params 1000 BUCKETSIZE 8 MAXITERATIONS 20 EXPANSION 2
OK
{{< / highlight >}}
