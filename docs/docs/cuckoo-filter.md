---
title: Cuckoo filter
linkTitle: Cuckoo filter
description: Cuckoo filter is a probabilistic data structure that checks for presence of an element in a set
type: docs
weight: 20
stack: true
---

# Cuckoo Filters

A Cuckoo filter, just like a Bloom filter, is a probabilistic data structure in Redis Stack that enables you to check if an element is present in a set in a very fast and space efficient way, while also allowing for deletions and showing better performance than Bloom in some scenarios.

While the Bloom filter is a bit array with flipped bits at positions decided by the hash function, a Cuckoo filter is an array of buckets, storing fingerprints of the values in one of the buckets at positions decided by the two hash functions. A membership query for item `x` searches the possible buckets for the fingerprint of `x`, and returns true if an identical fingerprint is found. A cuckoo filter's fingerprint size will directly determine the false positive rate.


## Use cases

**Targeted ad campaigns (advertising, retail)** 

This application answers this question: Has the user signed up for this campaign yet?

Use a Cuckoo filter for every campaign, populated with targeted users' ids. On every visit, the user id is checked against one of the Cuckoo filters. 

- If yes, the user has not signed up for campaign. Show the ad.
- If the user clicks ad and signs up, remove the user id from that Cuckoo filter. 
- If no, the user has signed up for that campaign. Try the next ad/Cuckoo filter. 
 
**Discount code/coupon validation (retail, online shops)** 

This application answers this question: Has this discount code/coupon been used yet?

Use a Cuckoo filter populated with all discount codes/coupons. On every try, the entered code is checked against the filter. 

- If no, the coupon is not valid. 
- If yes, the coupon can be valid. Check the main database. If valid, remove from Cuckoo filter as `used`.

Note> In addition to these two cases, Cuckoo filters serve very well all the Bloom filter use cases.

## Examples

> You'll learn how to create an empty cuckoo filter with an initial capacity for 1,000 items, add items, check their existence, and remove them. Even though the `CF.ADD` command can create a new filter if one isn't present, it might not be optimally sized for your needs. It's better to use the `CF.RESERVE` command to set up a filter with your preferred capacity.

{{< clients-example cuckoo_tutorial cuckoo >}}
> CF.RESERVE bikes:models 1000000
OK
> CF.ADD bikes:models "Smoky Mountain Striker"
(integer) 1
> CF.EXISTS bikes:models "Smoky Mountain Striker"
(integer) 1
> CF.EXISTS bikes:models "Terrible Bike Name"
(integer) 0
> CF.DEL bikes:models "Smoky Mountain Striker"
(integer) 1
{{< /clients-example >}}

## Bloom vs. Cuckoo filters
Bloom filters typically exhibit better performance and scalability when inserting
items (so if you're often adding items to your dataset, then a Bloom filter may be ideal).
Cuckoo filters are quicker on check operations and also allow deletions.

## Sizing Cuckoo filters

These are the main parameters and features of a cuckoo filter:

- `p` target false positive rate  
- `f` fingerprint length in bits
- `α` fill rate or load factor (0≤α≤1)
- `b` number of entries per bucket
- `m` number of buckets
- `n` number of items
- `C` average bits per item

Let's start by remembering that a cuckoo filter bucket can have multiple entries (where each entry stores one fingerprint). If we end up having all entries occupied with a fingerprint then we won't have empty slots to save new elements and the filter will be declared full, that's why we should always maintain a certain percentage of our cuckoo filter free.  
As a result of this the "real" memory cost of an item should include that overhead in addition to the fingerprint size. If `α` is the load factor (fingerprint size / total filter size) and `f` is the number of bits in an entry the amortised space cost `f/α bits`.

When you initialise a new filter you are asked to choose its capacity and bucket size. 

```
CF.RESERVE {key} {capacity} [BUCKETSIZE bucketSize] [MAXITERATIONS maxIterations]
[EXPANSION expansion]
``` 

### Choosing the capacity  (`capacity`)

The capacity of a Cuckoo filter is calculated as

```
capacity = n*f/α
```
where `n` is the number of elements you expect to have in your filter, `f` is the fingerprint length in bits which is set to `8` and `α` is the fill factor. So in order to get your filter capacity you must first choose a fill factor. The fill factor will determine the density of your data and of course the memory. 
The capacity will be rounded up to the next "power of two (2<sup>n</sup>)" number.

> Please note that inserting repeated items in a cuckoo filter will try to add them multiple times causing your filter to fill up

Because of how Cuckoo Filters work, the filter is likely to declare itself full before capacity is reached and therefore fill rate will likely never reach 100%.


### Choosing the bucket size (`BUCKETSIZE`)
Number of items in each bucket. A higher bucket size value improves the fill rate but also causes a higher error rate and slightly slower performance.

```
error_rate = (buckets * hash_functions)/2^fingerprint_size = (buckets*2)/256
```

When bucket size of 1 is used the fill rate is 55% and false positive error rate is 2/256 ≈ 0.78% **which is the minimal false positive rate you can achieve**. Larger buckets increase the error rate linearly but improve the fill rate of the filter. For example, a bucket size of 3 yields a 2.34% error rate and 80% fill rate. Bucket size of 4 - a 3.12% error rate and a 95% fill rate. 

### Choosing the scaling factor (`EXPANSION`)

When the filter self-declares itself full, it will auto-expand by generating additional sub-filters at the cost of reduced performance and increased error rate. The new sub-filter is created with size of the previous sub-filter multiplied by `EXPANSION` (chosen on filter creation). Like bucket size, additional sub-filters grow the error rate linearly (the compound error is a sum of all subfilters' errors). The size of the new sub-filter is the size of the last sub-filter multiplied by expansion and this is something very important to keep in mind. If you know you'll have to scale at some point it's better to choose a higher expansion value. The default is 1.

Maybe you're wondering "Why would I create a smaller filter with a high expansion rate if I know I'm going to scale anyway?"; the answer is: for cases where you need to keep many filters (let's say a filter per user, or per product) and most of them will stay small, but some with more activity will have to scale. 

The expansion factor will be rounded up to the next "power of two (2<sup>n</sup>)" number.

### Choosing the maximum number of iterations (`MAXITERATIONS`)
`MAXITERATIONS` dictates the number of attempts to find a slot for the incoming fingerprint. Once the filter gets full, a high MAXITERATIONS value will slow down insertions. The default value is 20.

### Interesting facts: 
- Unused capacity in prior sub-filters is automatically used when possible. 
- The filter can grow up to 32 times. 
- You can delete items to stay within filter limits instead of rebuilding
- Adding the same element multiple times will create multiple entries, thus filling up your filter.


## Performance
Adding an element to a Cuckoo filter has a time complexity of O(1).

Similarly, checking for an element and deleting an element also has a time complexity of O(1).



## Academic sources
- [Cuckoo Filter: Practically Better Than Bloom](https://www.cs.cmu.edu/~dga/papers/cuckoo-conext2014.pdf)
