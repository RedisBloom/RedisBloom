---
title: "Commands"
linkTitle: "Commands"
weight: 2
description: >
    Commands Overview
---

## Probabilistic Data Structures - Commands

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
