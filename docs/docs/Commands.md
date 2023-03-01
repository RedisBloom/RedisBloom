---
title: "Commands"
linkTitle: "Commands"
weight: 2
description: >
    Commands Overview
---

## Overview

RedisBloom adds a set of probabilistic data structures to Redis, including Bloom filter, Cuckoo filter, Count-min sketch, Top-K, and t-digest. Using this capability, you can query streaming data without needing to store all the elements of the stream. Probabilistic data structures each answer the following questions:

- Bloom filter and Cuckoo filter:
  -  Did value _v_ already appear in the data stream?
- Count-min sketch:
  - How many times did value _v_ appear in the data stream?
- Top-k:
  - What are the _k_ most frequent values in the data stream?
- t-digest:
  - Which fraction of the values in the data stream are smaller than a given value?
  - How many values in the data stream are smaller than a given value?
  - Which value is smaller than _p_ percent of the values in the data stream? (What is the _p_-percentile value?)
  - What is the mean value between the _p1_-percentile value and the _p2_-percentile value?
  - What is the value of the *n*ᵗʰ smallest/largest value in the data stream? (What is the value with [reverse] rank _n_?)

Answering each of these questions accurately can require a huge amount of memory, but you can lower the memory requirements drastically at the cost of reduced accuracy. Each of these data structures allows you to set a controllable trade-off between accuracy and memory consumption. In addition to having a smaller memory footprint, probabilistic data structures are generally much faster than accurate algorithms.

### Probabilistic Data Structures - Commands

*   [Bloom filter - commands](/commands/?name=bf.)
*   [Cuckoo filter - commands](/commands/?name=cf.)
*   [Count-min sketch - commands](/commands/?name=cms.)
*   [Top-k list - commands](/commands/?name=topk.)
*   [t-digest - commands](/commands/?name=tdigest.)

The details also include the syntax for the commands, where:

*   Command and subcommand names are in uppercase, for example `BF.RESERVE` or `BF.ADD`
*   Optional arguments are enclosed in square brackets, for example `[NONSCALING]`
*   Additional optional arguments, such as multiple element for insertion or query, are indicated by three period characters, for example `...`

Commands usually require a key's name as their first argument and an element to add or to query.

### Complexity

Some commands has complexity `O(k)` which indicated the number of hash functions. The number of hash functions is constant and complexity may be considered `O(1)`.
