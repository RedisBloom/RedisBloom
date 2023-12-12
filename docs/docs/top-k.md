---
title: Top-K
linkTitle: Top-K
description: Top-K is a probabilistic data structure that allows you to find the most frequent items in a data stream.
type: docs
stack: true
weight: 50
---

Top K is a probabilistic data structure in Redis Stack used to estimate the `K` highest-rank elements from a stream.

"Highest-rank" in this case means "elements with a highest number or score attached to them", where the score can be a count of how many times the element has appeared in the stream - thus making the data structure perfect for finding the elements with the highest frequency in a stream.
One very common application is detecting network anomalies and DDoS attacks where Top K can answer the question: Is there a sudden increase in the flux of requests to the same address or from the same IP?
 
There is, indeed, some overlap with the functionality of Count-Min Sketch, but the two data structures have their differences and should be applied for different use cases. 

The Redis Stack implementation of Top-K is based on the [HeavyKeepers](https://www.usenix.org/conference/atc18/presentation/gong) algorithm presented by Junzhi Gong et al. It discards some older approaches like "count-all" and "admit-all-count-some" in favour of a "**count-with-exponential-decay**" strategy which is biased against mouse (small) flows and has a limited impact on elephant (large) flows. This implementation uses two data structures in tandem: a hash table that holds the probabilistic counts (much like the Count-Min Sketch), and a min heap that holds the `K` items with the highest counts. This ensures high accuracy with shorter execution times than previous probabilistic algorithms allowed, while keeping memory utilization to a fraction of what is typically required by a Sorted Set. It has the additional benefit of being able to get real time notifications when elements are added or removed from the Top K list. 

## Use case

**Trending hashtags (social media platforms, news distribution networks)** 

This application answers these questions: 

- What are the K hashtags people have mentioned the most in the last X hours? 
- What are the K news with highest read/view count today? 

Data flow is the incoming social media posts from which you parse out the different hashtags. 

The `TOPK.LIST` command has a time complexity of `O(K)` so if `K` is small, there is no need to keep a separate set or sorted set of all the hashtags. You can query directly from the Top K itself. 

## Example

This example will show you how to track key words used "bike" when shopping online; e.g., "bike store" and "bike handlebars". Proceed as follows.
​
* Use `TOPK.RESERVE` to initialize a top K sketch with specific parameters. Note: the `width`, `depth`, and `decay_constant` parameters can be omitted, as they will be set to the default values 7, 8, and 0.9, respectively, if not present.
​
 ```
 > TOPK.RESERVE key k width depth decay_constant
 ```
 
 * Use `TOPK.ADD` to add items to the sketch. As you can see, multiple items can be added at the same time. If an item is returned when adding additional items, it means that item was demoted out of the min heap of the top items, below it will mean the returned item is no longer in the top 5, otherwise `nil` is returned. This allows dynamic heavy-hitter detection of items being entered or expelled from top K list.
​
In the example below, "pedals" displaces "handlebars", which is returned after "pedals" is added. Also note that the addition of both "store" and "seat" a second time don't return anything, as they're already in the top K.
 
 * Use `TOPK.LIST` to list the items entered thus far.
​
 * Use `TOPK.QUERY` to see if an item is on the top K list. Just like `TOPK.ADD` multiple items can be queried at the same time.
{{< clients-example topk_tutorial topk >}}
> TOPK.RESERVE bikes:keywords 5 2000 7 0.925
OK
> TOPK.ADD bikes:keywords store seat handlebars handles pedals tires store seat
1) (nil)
2) (nil)
3) (nil)
4) (nil)
5) (nil)
6) handlebars
7) (nil)
8) (nil)
> TOPK.LIST bikes:keywords
1) store
2) seat
3) pedals
4) tires
5) handles
> TOPK.QUERY bikes:keywords store handlebars
1) (integer) 1
2) (integer) 0
{{< /clients-example >}}

## Sizing

Choosing the size for a Top K sketch is relatively easy, because the only two parameters you need to set are a direct function of the number of elements (K) you want to keep in your list.

If you start by knowing your desired `k` you can easily derive the width and depth:

```
width = k*log(k)
depth =  log(k)  # but a minimum of 5
```

For the `decay_constant` you can use the value `0.9` which has been found as optimal in many cases, but you can experiment with different values and find what works best for your use case.

## Performance
Insertion in a top-k has time complexity of O(K + depth) ≈ O(K) and lookup has time complexity of O(K), where K is the number of top elements to be kept in the list and depth is the number of hash functions used.


## Academic sources
- [HeavyKeeper: An Accurate Algorithm for Finding Top-k Elephant Flows.](https://yangtonghome.github.io/uploads/HeavyKeeper_ToN.pdf)

## References
- [Meet Top-K: an Awesome Probabilistic Addition to RedisBloom](https://redis.com/blog/meet-top-k-awesome-probabilistic-addition-redisbloom/)
