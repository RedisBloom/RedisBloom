---
title: t-digest
linkTitle: t-digest
description: t-digest is a probabilistic data structure that allows you to estimate the percentile of a data stream.
type: docs
stack: true
weight: 40
---

The t-digest is a sketch data structure in Redis Stack for estimating percentiles from a data stream or a large dataset using a compact sketch.

It can answer questions like:
- Which fraction of the values in the data stream are smaller than a given value?
- How many values in the data stream are smaller than a given value?
- What's the highest value that's smaller than *p* percent of the values in the data stream? (what is the p-percentile value)?


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

We'll demonstrate creating a t-digest with an initial compression of 100, adding items to it, and checking the estimated percentile of a value. Note that the `COMPRESSION` argument is used to specify the tradeoff between accuracy and memory consumption. The default is 100. Higher values mean more accuracy. Note also that unlike some of the other probabilistic data structures, the `TDIGEST.ADD` command will not create a new structure if the key does not exist.

{{< clients-example tdigest_tutorial tdig_start >}}
> TDIGEST.CREATE bikes:sales COMPRESSION 100
OK
> TDIGEST.ADD bikes:sales 21
OK
> TDIGEST.ADD bikes:sales 150 95 75 34
OK
{{< /clients-example >}}


You can repeat calling [TDIGEST.ADD](https://redis.io/commands/tdigest.add/) whenever new observations are available

#### Estimating fractions or ranks by values

Another helpful feature in t-digest is CDF (definition of rank) which gives us the fraction of observations smaller or equal to a certain value. This command is very useful to answer questions like "*What's the percentage of observations with a value lower or equal to X*".

>More precisely, `TDIGEST.CDF` will return the estimated fraction of observations in the sketch that are smaller than X plus half the number of observations that are equal to X. We can also use the `TDIGEST.RANK` command, which is very similar. Instead of returning a fraction, it returns the **number** of observations in the sketch that are smaller than X plus half the number of observations that are equal to X, or in other words - the estimated rank of a value. The `TDIGEST.RANK` command is also variadic.

Let's illustrate this with an example: if we have a set of observations of people's age with gaussian distribution, we can ask a question like "What's the percentage of bike racers are younger than 50 years?"

{{< clients-example tdigest_tutorial tdig_cdf >}}
> TDIGEST.CREATE racer_ages
OK
> TDIGEST.ADD racer_ages 45.88 44.2 58.03 19.76 39.84 69.28 50.97 25.41 19.27 85.71 42.63
OK
> TDIGEST.CDF racer_ages 50
1) "0.63636363636363635"
> TDIGEST.RANK racer_ages 50
1) (integer) 7
> TDIGEST.RANK racer_ages 50 40
1) (integer) 7
2) (integer) 4
{{< /clients-example >}}


And lastly, `TDIGEST.REVRANK key value...` is similar to [TDIGEST.RANK](https://redis.io/commands/tdigest.rank/), but returns, for each input value, an estimation of the number of (observations larger than a given value + half the observations equal to the given value).


#### Estimating values by fractions or ranks

`TDIGEST.QUANTILE key fraction...` returns, for each input fraction, an estimation of the value (floating point) that is smaller than the given fraction of observations. `TDIGEST.BYRANK key rank...` returns, for each input rank, an estimation of the value (floating point) with that rank.

{{< clients-example tdigest_tutorial tdig_quant >}}
> TDIGEST.QUANTILE racer_ages .5
1) "44.200000000000003"
> TDIGEST.BYRANK racer_ages 4
1) "42.630000000000003"
{{< /clients-example >}}

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

{{< clients-example tdigest_tutorial tdig_min >}}
> TDIGEST.MIN racer_ages
"19.27"
> TDIGEST.MAX racer_ages
"85.709999999999994"
{{< /clients-example >}}

Both return nan when the sketch is empty.

Both commands return accurate results and are equivalent to `TDIGEST.BYRANK racer_ages 0` and `TDIGEST.BYREVRANK racer_ages 0` respectively.

Use `TDIGEST.INFO racer_ages` to retrieve some additional information about the sketch.

#### Resetting a sketch

{{< clients-example tdigest_tutorial tdig_reset >}}
> TDIGEST.RESET racer_ages
OK
{{< /clients-example >}}

## Academic sources
- [The _t_-digest: Efficient estimates of distributions](https://www.sciencedirect.com/science/article/pii/S2665963820300403)

## References
- [t-digest: A New Probabilistic Data Structure in Redis Stack](https://redis.com/blog/t-digest-in-redis-stack/)
