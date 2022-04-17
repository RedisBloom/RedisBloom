Create a Cuckoo Filter as `key` with a single sub-filter for the initial amount
of `capacity` for items. Because of how Cuckoo Filters work, the filter is
likely to declare itself full before `capacity` is reached and therefore fill
rate will likely never reach 100%. The fill rate can be improved by using a
larger `bucketsize` at the cost of a higher error rate.
When the filter self-declare itself `full`, it will auto-expand by generating
additional sub-filters at the cost of reduced performance and increased error
rate. The new sub-filter is created with size of the previous sub-filter
multiplied by `expansion`.
Like bucket size, additional sub-filters grow the error rate linearly.
The size of the new sub-filter is the size of the last sub-filter multiplied by
`expansion`. The default value is 1.

The minimal false positive error rate is 2/255 ≈ 0.78% when bucket size of 1 is
used. Larger buckets increase the error rate linearly (for example, a bucket size
of 3 yields a 2.35% error rate) but improve the fill rate of the filter.

`maxiterations` dictates the number of attempts to find a slot for the incoming
fingerprint. Once the filter gets full, high `maxIterations` value will slow
down insertions. The default value is 20.

Unused capacity in prior sub-filters is automatically used when possible.
The filter can grow up to 32 times.

## Parameters:

* **key**: The key under which the filter is found.
* **capacity**: Estimated capacity for the filter. Capacity is rounded to the
next `2^n` number. The filter will likely not fill up to 100% of it's capacity.
Make sure to reserve extra capacity if you want to avoid expansions.

Optional parameters:

* **bucketsize**: Number of items in each bucket. A higher bucket size value
improves the fill rate but also causes a higher error rate and slightly slower
performance.
* **maxiterations**: Number of attempts to swap items between buckets before
declaring filter as full and creating an additional filter. A low value is
better for performance and a higher number is better for filter fill rate.
* **expansion**: When a new filter is created, its size is the size of the
current filter multiplied by `expansion`. Expansion is rounded to the next
`2^n` number.

@return

@simple-string-reply - `OK` if executed correctly, or @error-reply otherwise.

@examples

```
redis> CF.RESERVE cf 1000
OK
```

```
redis> CF.RESERVE cf 1000
(error) ERR item exists
```

```
redis> CF.RESERVE cf_params 1000 BUCKETSIZE 8 MAXITERATIONS 20 EXPANSION 2
OK
```