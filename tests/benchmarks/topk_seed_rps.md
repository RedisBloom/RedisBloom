# TopK seed throughput versus master

Measured 2026-09-28 on Apple M4 Max, 36 GiB RAM, macOS.

## Conclusion

A possible small performance regression deserves follow-up. Pipelined INCRBY
was slower on both transports: default seed -0.84% TCP / -1.37% Unix socket;
random seed -1.66% / -2.69%. Pipelined COUNT for absent items was also slower:
both seed modes -2.15% TCP / -1.36% Unix socket. This is not a clean
no-regression verdict.

Server CPU medians generally agree with those small differences. INCRBY used
0.712/0.714/0.721 microseconds per request over TCP (master/default/random), and
0.579/0.587/0.594 over Unix sockets. COUNT-absent used 0.544/0.554/0.556 over TCP
and 0.432/0.437/0.441 over Unix sockets. The experiment does not isolate the
cause to seed arithmetic, the heap safety check, hash-dependent behavior, or
system variance. A controlled-host rerun is needed before declaring a confirmed
regression or optimizing production code.

Other pipelined differences were small and mixed. Non-pipelined results were
highly unstable: TCP INCRBY medians suggested 40–44% slowdowns but Unix sockets
suggested improvements. Default-seed COUNT-present was 14–15% slower in both
non-pipelined comparisons; its CPU cost did not show a matching increase
(TCP master/default 6.124/5.981 microseconds, Unix 2.114/2.125). All those samples
are retained below. The cause of the large wall-time stalls was not established.

This desktop was not idle; there are only five samples per case. Results are
not a statistical significance test or a performance release gate. This work
also does not resolve the earlier TopK accuracy-test failures.

## Method

- Master: `a89aac8e75da4a1fbbb59a307ea2f3b4ae87870d` (freshly fetched origin/master).
- Branch: `24994c34dda38ddeba3f9d1c808a6bfae75d151e`.
- macOS arm64 release modules; baseline TopK sources/build definition checked against master.
- Same Redis server/redis-benchmark binaries as prior comparisons; core revision `2c32610816636706b2203ef1a2b534a08d3a643f`.
- Reused `bloom_seed_rps.py --filter TOPK`; no production changes.
- Initialization: `TOPK.RESERVE key 100`, matching the existing TopK benchmark. Runtime checks verify K=100, width=8, depth=7, decay=0.9.
- 16 clients; pipeline depths 1 and 32; 500,000 commands per sample; five rounds.
- Modes: master, branch with omitted seed (legacy row/fingerprint seed scheme), branch with `SEED random`.
- Fresh isolated server per mode/round; persistence disabled; existing Redis instances untouched.
- Mode order rotates each round; redis-benchmark workload RNG seed fixed at 12345. Per-key random hash seeds and TopK's internal rand()-based decay are not fixed. Mutation trajectories need not be identical across processes.
- INCRBY: fresh empty sketch per sample; one increment of 1 per command; random items from a billion-item domain. This is a mostly-new-item, high-churn workload, not a skewed heavy-hitter stream.
- Read sketch: insert exactly 100 distinct fixed items once. TOPK.LIST and TOPK.QUERY must confirm all 100 are retained before timing. Membership is unchanged after every read-only sample.
- QUERY and COUNT each have present-item and absent-item workloads. Queries select uniformly from the 100 retained items or 100 never-inserted names with a different prefix. COUNT is an estimate, not an exact-count accuracy check.
- INFO commandstats checks that each measured command executes exactly 500,000 times with zero failed/rejected calls. TOPK.INFO and membership checks run outside timing.
- 50,000 warm-up queries per process; initialization, prefill, warm-up and validation excluded from reported RPS. CPU deltas include the negligible extra INFO commandstats snapshot before timing.
- All samples retained. RPS is commands/second, one item per command.
- 300 samples / 150 million measured commands across both transports, excluding warm-up/prefill.
- Not covered: other K/width/depth/decay settings, skewed or changing-leader streams, large increment weights, TOPK.ADD separately, TOPK.LIST throughput, measured multi-item batches, reserve/random-generation cost, manual seeds separately, persistence/replication load, long/binary values, or other hardware.

## Median RPS

Negative change means slower than master. Percentages use unrounded values.

### TCP

| Pipeline | Operation | Master | Default | Change | Random | Change |
|---|---|---:|---:|---:|---:|---:|
| 1 | incrby | 117,758 | 66,225 | -43.76% | 70,087 | -40.48% |
| 1 | query-present | 69,099 | 67,558 | -2.23% | 123,946 | +79.38% |
| 1 | query-absent | 71,757 | 73,035 | +1.78% | 66,988 | -6.65% |
| 1 | count-present | 77,220 | 66,146 | -14.34% | 93,406 | +20.96% |
| 1 | count-absent | 65,045 | 70,304 | +8.08% | 120,540 | +85.32% |
| 32 | incrby | 1,404,494 | 1,392,758 | -0.84% | 1,381,216 | -1.66% |
| 32 | query-present | 1,886,793 | 1,893,939 | +0.38% | 1,879,699 | -0.38% |
| 32 | query-absent | 1,858,736 | 1,879,699 | +1.13% | 1,858,736 | +0.00% |
| 32 | count-present | 1,798,561 | 1,811,594 | +0.72% | 1,785,714 | -0.71% |
| 32 | count-absent | 1,831,502 | 1,792,115 | -2.15% | 1,792,115 | -2.15% |

### Unix socket

| Pipeline | Operation | Master | Default | Change | Random | Change |
|---|---|---:|---:|---:|---:|---:|
| 1 | incrby | 261,506 | 313,480 | +19.87% | 284,900 | +8.95% |
| 1 | query-present | 312,110 | 303,398 | -2.79% | 321,130 | +2.89% |
| 1 | query-absent | 278,396 | 319,693 | +14.83% | 323,625 | +16.25% |
| 1 | count-present | 322,165 | 272,926 | -15.28% | 322,789 | +0.19% |
| 1 | count-absent | 116,333 | 324,254 | +178.73% | 307,125 | +164.00% |
| 32 | incrby | 1,730,104 | 1,706,485 | -1.37% | 1,683,502 | -2.69% |
| 32 | query-present | 2,380,953 | 2,403,846 | +0.96% | 2,358,491 | -0.94% |
| 32 | query-absent | 2,358,491 | 2,392,345 | +1.44% | 2,304,148 | -2.30% |
| 32 | count-present | 2,336,449 | 2,314,815 | -0.93% | 2,272,727 | -2.73% |
| 32 | count-absent | 2,304,148 | 2,272,727 | -1.36% | 2,272,727 | -1.36% |

## Reproduce

From the RedisBloom root, with redis-py installed and release builds of both modules:

```sh
python3 tests/benchmarks/bloom_seed_rps.py --filter TOPK \
  --server /path/to/redis-server \
  --benchmark /path/to/redis-benchmark \
  --master /path/to/master/redisbloom.so \
  --branch /path/to/branch/redisbloom.so
```

Repeat with `--unix-socket`. JSON output includes every sample, server CPU time
per request, and median summaries. Both runs completed successfully, including
all command-count, error-count, configuration and membership assertions.

## Raw RPS samples

Whole RPS, rounds 1–5, including every outlier.

| Transport | Pipeline | Operation | Mode | Five samples |
|---|---|---|---|---|
| TCP | 1 | incrby | master | 123640, 68559, 78939, 117758, 130378 |
| TCP | 1 | incrby | default | 127779, 63939, 66225, 66041, 66827 |
| TCP | 1 | incrby | random | 63395, 71225, 70087, 124969, 66155 |
| TCP | 1 | query-present | master | 69099, 130446, 116063, 65720, 67431 |
| TCP | 1 | query-present | default | 67558, 63307, 66225, 119674, 124440 |
| TCP | 1 | query-present | random | 123946, 127291, 135943, 63068, 68700 |
| TCP | 1 | query-absent | master | 125913, 71013, 71757, 65703, 136575 |
| TCP | 1 | query-absent | default | 62688, 125945, 125881, 63873, 73035 |
| TCP | 1 | query-absent | random | 70254, 66988, 64045, 65342, 142857 |
| TCP | 1 | count-present | master | 69454, 117288, 77220, 128667, 69852 |
| TCP | 1 | count-present | default | 127649, 64450, 66146, 64691, 125219 |
| TCP | 1 | count-present | random | 73014, 123671, 93406, 128074, 65902 |
| TCP | 1 | count-absent | master | 64516, 65045, 122669, 62313, 67558 |
| TCP | 1 | count-absent | default | 70304, 64325, 128172, 122220, 63792 |
| TCP | 1 | count-absent | random | 124626, 77268, 120540, 65694, 135943 |
| TCP | 32 | incrby | master | 1388889, 1416431, 1385042, 1412429, 1404494 |
| TCP | 32 | incrby | default | 1388889, 1408451, 1392758, 1385042, 1412429 |
| TCP | 32 | incrby | random | 1381216, 1388889, 1366120, 1412429, 1355014 |
| TCP | 32 | query-present | master | 1865672, 2000000, 1879699, 1901141, 1886793 |
| TCP | 32 | query-present | default | 1893939, 1908397, 1754386, 1845019, 1923077 |
| TCP | 32 | query-present | random | 1879699, 1908397, 1845019, 1960784, 1838235 |
| TCP | 32 | query-absent | master | 1858736, 1805054, 1831502, 1915709, 1901141 |
| TCP | 32 | query-absent | default | 1879699, 1893939, 131441, 135648, 1923077 |
| TCP | 32 | query-absent | random | 1838235, 1865672, 1858736, 1886793, 1858736 |
| TCP | 32 | count-present | master | 1872659, 1779360, 1798561, 1831502, 1798561 |
| TCP | 32 | count-present | default | 1851852, 1851852, 1529052, 1811594, 1672241 |
| TCP | 32 | count-present | random | 1805054, 1694915, 1785714, 1858736, 116741 |
| TCP | 32 | count-absent | master | 1858736, 1754386, 1754386, 1838235, 1831502 |
| TCP | 32 | count-absent | default | 1792115, 1851852, 1453488, 1824818, 1666667 |
| TCP | 32 | count-absent | random | 1792115, 1805054, 1779360, 1824818, 1748252 |
| Unix socket | 1 | incrby | master | 319898, 308261, 100664, 261506, 109553 |
| Unix socket | 1 | incrby | default | 321337, 313480, 297974, 276702, 316857 |
| Unix socket | 1 | incrby | random | 284900, 106883, 306373, 111508, 320513 |
| Unix socket | 1 | query-present | master | 333333, 312110, 315060, 293427, 300300 |
| Unix socket | 1 | query-present | default | 100746, 303398, 321750, 123824, 330907 |
| Unix socket | 1 | query-present | random | 306185, 329381, 321130, 321958, 131822 |
| Unix socket | 1 | query-absent | master | 105955, 278396, 292398, 126454, 309598 |
| Unix socket | 1 | query-absent | default | 330688, 117786, 319693, 309406, 323415 |
| Unix socket | 1 | query-absent | random | 323625, 329164, 111359, 329598, 323415 |
| Unix socket | 1 | count-present | master | 326584, 318878, 322165, 328515, 314861 |
| Unix socket | 1 | count-present | default | 325945, 325521, 164312, 272926, 103778 |
| Unix socket | 1 | count-present | random | 105597, 322789, 308452, 325733, 325945 |
| Unix socket | 1 | count-absent | master | 277316, 99840, 116333, 327439, 113071 |
| Unix socket | 1 | count-absent | default | 330251, 324254, 322373, 314663, 326371 |
| Unix socket | 1 | count-absent | random | 330688, 298151, 307125, 100060, 329815 |
| Unix socket | 32 | incrby | master | 1724138, 1736111, 1730104, 1766785, 1724138 |
| Unix socket | 32 | incrby | default | 1748252, 1706485, 1677852, 1706485, 1672241 |
| Unix socket | 32 | incrby | random | 1736111, 1666667, 1689189, 1677852, 1683502 |
| Unix socket | 32 | query-present | master | 2380953, 2392345, 2164502, 2450981, 2336449 |
| Unix socket | 32 | query-present | default | 2475248, 2415459, 2369668, 2403846, 2369668 |
| Unix socket | 32 | query-present | random | 2380953, 215610, 2293578, 2358491, 2392345 |
| Unix socket | 32 | query-absent | master | 2358491, 2403846, 2283105, 2415459, 2293578 |
| Unix socket | 32 | query-absent | default | 149120, 2450981, 2392345, 2403846, 2392345 |
| Unix socket | 32 | query-absent | random | 2403846, 2304148, 2358491, 2272727, 2293578 |
| Unix socket | 32 | count-present | master | 2283105, 2358491, 2336449, 2358491, 2325581 |
| Unix socket | 32 | count-present | default | 2369668, 2293578, 2336449, 2283105, 2314815 |
| Unix socket | 32 | count-present | random | 2369668, 2336449, 2272727, 2242153, 2252252 |
| Unix socket | 32 | count-absent | master | 2314815, 2358491, 2304148, 2283105, 2183406 |
| Unix socket | 32 | count-absent | default | 2358491, 2293578, 2272727, 2262444, 2252252 |
| Unix socket | 32 | count-absent | random | 2336449, 2336449, 2252252, 2272727, 1992032 |

