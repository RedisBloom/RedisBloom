---
title: Count-min sketch
linkTitle: Count-min sketch
description: Count-min sketch is a probabilistic data structure that allows you to count the number of times an item appears in a data stream.
type: docs
stack: true
weight: 60
---

# Count-min sketch

Count-Min Sketch is a probabilistic data structure that estimates the frequency of events/elements in a stream of data.

It is  similar to Bloom filters in a way that it uses multiple hash functions to map an element onto an array, but instead of a single bit array is consists of a two-dimensional array with as many number of rows as there are hash functions. The chosen number of rows and columns determines the error rates.   

It uses a sub-linear space at the expense of over-counting some events due to collisions. It consumes a stream of events/elements and keeps estimated counters of their frequency.

It is very important to know that the results coming from a Count-Min sketch lower than a certain threshold (determined by the error_rate) should be ignored and often even approximated to zero. So Count-Min sketch is indeed a data-structure for counting frequencies of elements in a stream, but it's only useful for higher counts. Very low counts should be ignored as noise.


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


## Examples
Let's say we choose an error of 0.1%(`0.001`) with certainty of 99.8%(`0.998`) (thus probability of error 0.02% (`0.002`)). The resulting sketch will try to keep the error within 0.1% of the sum of counts of **ALL** elements that have been added to the sketch and the probability for this error to be higher  than that (a collision of an element below the threshold with an element above the threshold) will be 0.02%. 

```
> CMS.INITBYPROB key 0.001 0.002
```

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


## Academic sources
- [An Improved Data Stream Summary: The Count-Min Sketch and its Applications](http://dimacs.rutgers.edu/~graham/pubs/papers/cm-full.pdf)

## References
- [Count-Min Sketch: The Art and Science of Estimating Stuff](https://redis.com/blog/count-min-sketch-the-art-and-science-of-estimating-stuff/)