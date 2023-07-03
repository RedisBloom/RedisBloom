---
title: t-digest
linkTitle: t-digest
description: t-digest is a probabilistic data structure that allows you to estimate the percentile of a data stream.
type: docs
stack: true
weight: 40
---

The t-digest is a sketch data structure in Redis Stack for estimating quantiles from a data stream or a large dataset using a compact sketch.

It can answer questions like:
- Which fraction of the values in the data stream are smaller than a given value?
- How many values in the data stream are smaller than a given value?
- What's the highest value that's smaller than *p* percent of the values in the data stream? (what is the p-percentile value)?

### What is a quantile?
Before digging into t-digest, we need to understand quantiles and percentiles. The word **“quantile”** comes from the word quantity. In simple terms, quantiles are the points where a sample is divided into equal-sized groups. It can also refer to dividing a probability distribution into areas of equal probability. Let's illustrate this with a simple example:

<img style="width: 100%;margin: auto;max-width: 1000px;" src="/docs/data-types/probabilistic/images/quantiles.png" alt="Quantile points on an axis">

Above we see 12 data points divided into four groups of an equal number of elements (3). The values that divide those groups are called quantiles. The 0.25 quantile has 25% of all the data points to its left. The 0.5 quantile is the point that splits the data in two and is equal to the **median** value of the dataset.

**A percentile is a special quantile, where we divide the whole set in 100 groups (or hundred quantiles). So a 0.25 quantile would be a 25th percentile.** In the rest of the training we'll work with percentiles, because they're more intuitive to understand.

### What is t-digest?
t-digest is a data structure that will estimate a percentile point without having to store and order all the data points in a set. For example: to answer the question "What's the average latency for 99% of my database operations" we would have to store the average latency for every user, order the values, cut out the last 1% and only then find the average value of all the rest. This kind of process is costly not just in terms of the processing needed to order those values but also in terms of the space needed to store them. Those are precisely the problems t-digest solves.

t-digest can also be used to estimate other values related to percentiles, like trimmed means.   

> A **trimmed mean** is the mean value from the sketch, excluding observation values outside the low and high cutoff percentiles. For example, a 0.1 trimmed mean is the mean value of the sketch, excluding the lowest 10% and the highest 10% of the values.

## Use cases

**Hardware/software monitoring**

You measure your online server response latency, and you like to query:

- What are the 50th, 90th, and 99th percentiles of the measured latencies?

- Which fraction of the measured latencies are less than 25 milliseconds?

- What is the mean latency, ignoring outliers? or What is the mean latency between the 10th and the 90th percentile?

**Online gaming**

Millions of people are playing a game on your online gaming platform, and you want to give the following information to each player?

- Your score is better than x percent of the game sessions played.

- There were about y game sessions where people scored larger than you.

- To have a better score than 90% of the games played, your score should be z.

**Network traffic monitoring**

You measure the IP packets transferred over your network each second and try to detect denial-of-service attacks by asking:

- Does the number of packets in the last second exceed 99% of previously observed values?

- How many packets do I expect to see under _normal_ network conditions? 
(Answer: between x and y, where x represents the 1st percentile and y represents the 99th percentile).

**Predictive maintenance**

- Was the measured parameter (noise level, current consumption, etc.) irregular? (not within the [1st percentile...99th percentile] range)?

- To which values should I set my alerts?


## Examples

#### Creating a t-digest

```
> TDIGEST.CREATE my-tdigest COMPRESSION 100
```

The `COMPRESSION` argument is used to specify the tradeoff between accuracy and memory consumption. The default is 100. Higher values mean more accuracy.

#### Adding a single element to the t-digest:
```
> TDIGEST.ADD my-tdigest 20.9
```

#### Adding multiple elements to the t-digest:
```
> TDIGEST.ADD my-tdigest 308 315.9
```

You can repeat calling [TDIGEST.ADD](https://redis.io/commands/tdigest.add/) whenever new observations are available

#### Estimating fractions or ranks by values

Another helpful feature in t-digest is CDF (definition of rank) which gives us the fraction of observations smaller or equal to a certain value. This command is very useful to answer questions like "*What's the percentage of observations with a value lower or equal to X*".

>More precisely, `TDIGEST.CDF` will return the estimated fraction of observations in the sketch that are smaller than X plus half the number of observations that are equal to X

Let's illustrate this with an example: if we have a set of observations of people's age with gaussian distribution, we can ask a question like "What's the percentage of people younger than 50 years?"

```
> TDIGEST.ADD my-tdigest 45.88 44.2 58.03 19.76 39.84 69.28 50.97 25.41 19.27 85.71 42.63

> TDIGEST.CDF my-tdigest 50
```

The `TDIGEST.RANK` command is very similar to `TDIGEST.CDF` but instead of returning a fraction, it returns the **number** of observations in the sketch that are smaller than X plus half the number of observations that are equal to X, or in other words - the estimated rank of a value.

```
> TDIGEST.RANK my-tdigest 50
```

And lastly, `TDIGEST.REVRANK key value...` is similar to [TDIGEST.RANK](https://redis.io/commands/tdigest.rank/), but returns, for each input value, an estimation of the number of (observations larger than a given value + half the observations equal to the given value).


#### Estimating values by fractions or ranks

`TDIGEST.QUANTILE key fraction...` returns, for each input fraction, an estimation of the value (floating point) that is smaller than the given fraction of observations.

```
> TDIGEST.QUANTILE my-tdigest 0.5
```

`TDIGEST.BYRANK key rank...` returns, for each input rank, an estimation of the value (floating point) with that rank.

```
> TDIGEST.BYRANK my-tdigest 4
```

`TDIGEST.BYREVRANK key rank...` returns, for each input **reverse rank**, an estimation of the **value** (floating point) with that reverse rank.

#### Estimating trimmed mean

Use `TDIGEST.TRIMMED_MEAN key lowFraction highFraction` to retrieve an estimation of the mean value between the specified fractions.

This is especially useful for calculating the average value ignoring outliers. For example - calculating the average value between the 20th percentile and the 80th percentile.

#### Merging sketches

Sometimes it is useful to merge sketches. For example, suppose we measure latencies for 3 servers, and we want to calculate the 90%, 95%, and 99% latencies for all the servers combined.

`TDIGEST.MERGE destKey numKeys sourceKey... [COMPRESSION compression] [OVERRIDE]` merges multiple sketches into a single sketch.

If `destKey` does not exist - a new sketch is created.

If `destKey` is an existing sketch, its values are merged with the values of the source keys. To override the destination key contents, use `OVERRIDE`.

#### Retrieving sketch information

Use `TDIGEST.MIN` and `TDIGEST.MAX` to retrieve the minimal and maximal values in the sketch, respectively.

```
> TDIGEST.MIN my-tdigest
> TDIGEST.MAX my-tdigest
```

Both return nan when the sketch is empty.

Both commands return accurate results and are equivalent to `TDIGEST.BYRANK my-tdigest 0` and `TDIGEST.BYREVRANK my-tdigest 0` respectively.

Use `TDIGEST.INFO my-tdigest` to retrieve some additional information about the sketch.

#### Resetting a sketch

`TDIGEST.RESET my-tdigest`

## Academic sources
- [The _t_-digest: Efficient estimates of distributions](https://www.sciencedirect.com/science/article/pii/S2665963820300403)

## References
- [t-digest: A New Probabilistic Data Structure in Redis Stack](https://redis.com/blog/t-digest-in-redis-stack/)