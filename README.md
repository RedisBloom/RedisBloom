[![GitHub issues](https://img.shields.io/github/release/RedisLabsModules/redisbloom.svg)](https://github.com/RedisBloom/RedisBloom/releases/latest)
[![CircleCI](https://circleci.com/gh/RedisBloom/RedisBloom.svg?style=svg)](https://circleci.com/gh/RedisBloom/RedisBloom)
[![Dockerhub](https://img.shields.io/docker/pulls/redis/redis-stack-server?label=redis-stack-server)](https://img.shields.io/docker/pulls/redis/redis-stack-server)
[![codecov](https://codecov.io/gh/RedisBloom/RedisBloom/branch/master/graph/badge.svg)](https://codecov.io/gh/RedisBloom/RedisBloom)

# RedisBloom: Probabilistic Data Structures for Redis

[![Discord](https://img.shields.io/discord/697882427875393627?style=flat-square)](https://discord.gg/wXhwjCQ)

<img src="docs/docs/images/logo.svg" alt="logo" width="300"/>

> [!NOTE]
> Starting with Redis 8, all RedisBloom data structures are integral to Redis. You don't need to install this module separately.
>
> We no longer release standalone versions of RedisBloom.
>
> See https://github.com/redis/redis

> [!NOTE]
> 32 bit systems are not supported.

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

## Documentation

https://redis.io/docs/latest/develop/data-types/probabilistic/

### Top-K replication upgrade and rollback

On Redis 8.0 and later, Top-K writes replicate through the internal `TOPK._ADD_SEEDED`
and `TOPK._INCRBY_SEEDED` commands. Both primary and replicas must support these commands;
older replicas reject them and can lose writes or stop, depending on the configured
propagation error behavior. On older Redis servers, these commands are not registered
and writes replicate through the public commands without a seed. The original
probabilistic decay divergence remains on that fallback path.

Pause Top-K writes while module versions differ. Upgrade replicas before the primary,
and prevent failover to an upgraded node while any replica still uses the old module.
After all nodes are upgraded, fully resynchronize replicas from the primary before
resuming Top-K writes: this fix does not repair existing divergence.

Before rolling back the module on an AOF-enabled node, pause Top-K writes throughout
the topology, run `BGREWRITEAOF` on every node being downgraded, and wait for
`aof_rewrite_scheduled:0`, `aof_rewrite_in_progress:0`, and
`aof_last_bgrewrite_status:ok`. Keep writes paused until
rollback completes so new seeded commands cannot enter the AOF. The rewrite replaces
seeded command history with serialized state; Top-K RDB encoding remains 0. An older
module cannot load an AOF that still contains the seeded command names.

On replicas, Top-K write command statistics and latency metrics use the internal
command names, and ordinary `MONITOR` clients no longer see these writes. Primary
client commands keep their public names.

## License

Starting with Redis 8, RedisBloom is licensed under your choice of: (i) Redis Source Available License 2.0 (RSALv2); (ii) the Server Side Public License v1 (SSPLv1); or (iii) the GNU Affero General Public License version 3 (AGPLv3). Please review the license folder for the full license terms and conditions. Prior versions remain subject to (i) and (ii).

## Code contributions

By contributing code to this Redis module in any form, including sending a pull request via GitHub, a code fragment or patch via private email or public discussion groups, you agree to release your code under the terms of the Redis Software Grant and Contributor License Agreement. Please see the CONTRIBUTING.md file in this source distribution for more information. For security bugs and vulnerabilities, please see SECURITY.md. 
