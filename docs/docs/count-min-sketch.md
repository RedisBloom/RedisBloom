---
title: Count-min sketch
linkTitle: Count-min sketch
description: Count-min sketch is a probabilistic data structure that estimates the frequency of an element in a data stream.
type: docs
stack: true
weight: 60
---

# Count-min sketch

Count-Min Sketch is a probabilistic data structure in Redis Stack that can be used to estimate the frequency of events/elements in a stream of data.

It uses a sub-linear space at the expense of over-counting some events due to collisions. It consumes a stream of events/elements and keeps estimated counters of their frequency.

It is very important to know that the results coming from a Count-Min sketch lower than a certain threshold (determined by the error_rate) should be ignored and often even approximated to zero. So Count-Min sketch is indeed a data-structure for counting frequencies of elements in a stream, but it's only useful for higher counts. Very low counts should be ignored as noise.

## Use cases

**Products (retail, online shops)** 

This application answers this question: What was the sales volume (on a certain day) for a product? 

Use one Count-Min sketch created per day (period). Every product sale goes into the CMS. The CMS give reasonably accurate results for the products that contribute the most toward the sales. Products with low percentage of the total sales are ignored. 

## Examples
Assume you select an error rate of 0.1% (0.001) with a certainty of 99.8% (0.998). This means you have an error probability of 0.02% (0.002). Your sketch strives to keep the error within 0.1% of the total count of all elements you've added. There's a 0.02% chance the error might exceed this—like when an element below the threshold overlaps with one above it. When you add a few items to the CMS and evaluate their frequency, remember that in such a small sample, collisions are rare, as seen with other probabilistic data structures.

{{< clients-example cms_tutorial cms >}}
> CMS.INITBYPROB bikes:profit 0.001 0.002
OK
> CMS.INCRBY bikes:profit "Smokey Mountain Striker" 100
(integer) 100
> CMS.INCRBY bikes:profit "Rocky Mountain Racer" 200 "Cloudy City Cruiser" 150
1) (integer) 200
2) (integer) 150
> CMS.QUERY bikes:profit "Smokey Mountain Striker" "Rocky Mountain Racer" "Cloudy City Cruiser" "Terrible Bike Name"
1) (integer) 100
2) (integer) 200
3) (integer) 150
4) (integer) 0
> CMS.INFO bikes:profit
1) width
2) (integer) 2000
3) depth
4) (integer) 9
5) count
6) (integer) 450
{{< /clients-example >}}

##### Example 1:
If we had a uniform distribution of 1000 elements where each has a count of around 500 the threshold would be 500: 

```
threshold = error * total_count  = 0.001 * (1000*500) = 500
```

This shows that a CMS is maybe not the best data structure to count frequency of a uniformly distributed stream.
Let's try decreasing the error to 0.01%:

```
threshold = error * total_count  = 0.0001 * (1000*500) = 100
```
This threshold looks more acceptable already, but it means we will need a bigger sketch width `w = 2/error = 20 000` and consequently - more memory.

##### Example 2:
In another example let's imagine a normal (gaussian) distribution where we have 1000 elements, out of which 800 will have a summed count of 400K (with an average count of 500) and 200 elements will have a much higher summed count of 1.6M (with an average count of 8000), making them the heavy hitters (elephant flow). The threshold after "populating" the sketch with all the 1000 elements would be:

```
threshold = error * total_count = 0.001 * 2M = 2000
```

This threshold seems to be sitting comfortably between the 2 average counts 500 and 8000 so the initial chosen error rate should be working well for this case.


# Sizing

Even though the Count-Min sketch is similar to Bloom filter in many ways, its sizing is considerably more complex. The initialisation command receives only two sizing parameters, but you have to understand them thoroughly if you want to have a usable sketch.

```
CMS.INITBYPROB key error probability
```

### 1. Error

The `error` parameter will determine the width `w` of your sketch and the probability will determine the number of hash functions (depth `d`). The error rate we choose will determine the threshold above which we can trust the result from the sketch. The correlation is:
```
threshold = error * total_count 
```
or
```
error = threshold/total_count
```

where `total_count` is the sum of the count of all elements that can be obtained from the `count` key of the result of the `CMS.INFO` command and is of course dynamic - it changes with every new increment in the sketch. At creation time you can approximate the `total_count` ratio as a product of the average count you'll be expecting in the sketch and the average number of elements.

Since the threshold is a function of the total count in the filter it's very important to note that it will grow as  the count grows, but knowing the total count we can always dynamically calculate the threshold. If a result is below it - it can be discarded.

  
### 2. Probability
  
`probability` in this data structure represents the chance of an element that has a count below the threshold to collide with elements that had a count above the threshold on all sketches/depths thus returning a min-count of a frequently occurring element instead of its own.



## Performance
Adding, updating and querying for elements in a CMS has a time complexity O(1).


## Academic sources
- [An Improved Data Stream Summary: The Count-Min Sketch and its Applications](http://dimacs.rutgers.edu/~graham/pubs/papers/cm-full.pdf)

## References
- [Count-Min Sketch: The Art and Science of Estimating Stuff](https://redis.com/blog/count-min-sketch-the-art-and-science-of-estimating-stuff/)
