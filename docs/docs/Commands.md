---
title: "Commands"
linkTitle: "Commands"
type: docs
weight: 2
description: >
    Commands Overview
---

## Overview

### Supported Probabilistic Data Structures

Redis includes a single Probabilistic Data Structure (PDS), the HyperLogLog which is used to count distinct elements in a multiset. RedisBloom adds to redis five additional PDS. 
*   Bloom Filter - test for membership of elements in a set. 
*   Cuckoo Filter - test for membership of elements in a set. 
*   Count-Min Sketch - count the frequency of elements in a stream. 
*   TopK - maintain a list of most frequent K elements in a stream.
*   T-digest Sketch - query for quantile.

### Probabilistic Data Structures API

Details on module's [commands](/commands/?group=module) can be filtered for a specific PDS.
*   [`Bloom Filter commands`](/commands/?name=bf.).
*   [`Cuckoo Filter commands`](/commands/?name=cf.).
*   [`Count-Min Sketch commands`](/commands/?name=cms.).
*   [`TopK list commands`](/commands/?name=topk.).
*   [`T-digest Sketch commands`](/commands/?name=tdigest.).

The details also include the syntax for the commands, where:

*   Command and subcommand names are in uppercase, for example `BF.RESERVE` or `BF.ADD`
*   Optional arguments are enclosed in square brackets, for example `[NONSCALING]`
*   Additional optional arguments, such as multiple element for insertion or query, are indicated by three period characters, for example `...`

Commands usually require a key's name as their first argument and an element to add or to query.

### Complexity

Some commands has complexity `O(k)` which indicated the number of hash functions. The number of hash functions is constant and complexity may be considered `O(1)`.