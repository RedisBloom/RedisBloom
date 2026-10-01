# CMS seed throughput versus master

Measured 2026-09-28 on Apple M4 Max, 36 GiB RAM, macOS.

## Conclusion

No consistent pipelined slowdown was demonstrated across transports. Random-seed
queries were 3.17–3.45% slower over TCP, but the Unix-socket comparison showed
+2.02% for present-item queries and no median change for absent-item queries.
This does not establish that the TCP difference is harmless or that small
regressions are absent.

Non-pipelined increments warrant follow-up: default seed was 3.19% slower over
TCP and 3.28% slower over Unix sockets; random seed was 1.47% and 2.62% slower,
respectively. Treat this as a possible small regression, not a confirmed one.
There were substantial timing outliers on both master and branch, the desktop
was not idle, and each case has only five samples. This is not a clean
no-regression verdict or a performance release gate.

Server CPU medians provide additional context: non-pipelined increments used
6.177/6.150/6.142 microseconds per request over TCP (master/default/random) and
2.268/2.327/2.300 over Unix sockets. The increased CPU cost was not reproduced
across both transports. The cause of the stalls or small differences was not
established; dedicated-host measurements are needed to resolve them.

## Method

- Master: `a89aac8e75da4a1fbbb59a307ea2f3b4ae87870d` (freshly fetched origin/master).
- Branch: `24994c34dda38ddeba3f9d1c808a6bfae75d151e`.
- Both modules: macOS arm64 release builds; baseline CMS sources/build definition checked against master.
- Same Redis server/redis-benchmark binaries as Bloom/Cuckoo; core revision `2c32610816636706b2203ef1a2b534a08d3a643f`.
- Reused `bloom_seed_rps.py --filter CMS`; no production changes.
- 16 clients; pipeline depths 1 and 32; 500,000 requests per sample; five rounds.
- Modes: master, branch with omitted seed (legacy row seeds), branch with `SEED random`.
- Fresh isolated server per mode/round; persistence disabled; existing Redis instances untouched.
- Mode order rotates each round; fixed workload RNG seed 12345.
- Initialization: `CMS.INITBYPROB key 0.001 0.01`, matching the repository's existing CMS benchmark. Runtime checks confirm width 2000, depth 7, CELL_SIZE 4 (56,000 bytes of counters).
- INCRBY: fresh empty sketch per sample, one item incremented by 1 per command; random items from a billion-item domain.
- QUERY: 500,000 deterministic distinct items preloaded with count 1; present queries use that domain and absent queries use a separate prefix. CMS can return nonzero estimates for absent items; these are query-workload labels, not accuracy assertions.
- After each sample, CMS.INFO must show the exact expected total increment count and unchanged width/depth/cell size.
- 50,000 warm-up queries per server; initialization, prefill, warm-up and validation excluded from timed RPS.
- All samples retained, including stalls. Medians are commands/second, one item per command.
- 180 samples / 90 million measured commands across both transports, excluding prefill/warm-up.
- Not covered: other widths/depths/cell sizes, larger working sets, skewed workloads, negative increments, overflow paths, CMS.MERGE, measured multi-item batches, initialization/random-generation cost, manual seeds separately, persistence/replication load, or other hardware. This is a throughput test, not an accuracy test.

## Median RPS

Negative change means slower than master. Percentages use unrounded values.

### TCP

| Pipeline | Operation | Master | Default | Change | Random | Change |
|---|---|---:|---:|---:|---:|---:|
| 1 | incrby | 126,839 | 122,790 | -3.19% | 124,969 | -1.47% |
| 1 | query-present | 74,272 | 82,386 | +10.92% | 74,305 | +0.04% |
| 1 | query-absent | 71,613 | 77,483 | +8.20% | 111,259 | +55.36% |
| 32 | incrby | 1,424,501 | 1,449,275 | +1.74% | 1,428,572 | +0.29% |
| 32 | query-present | 1,639,344 | 1,633,987 | -0.33% | 1,587,302 | -3.17% |
| 32 | query-absent | 1,623,377 | 1,628,664 | +0.33% | 1,567,398 | -3.45% |

### Unix socket

| Pipeline | Operation | Master | Default | Change | Random | Change |
|---|---|---:|---:|---:|---:|---:|
| 1 | incrby | 320,308 | 309,789 | -3.28% | 311,915 | -2.62% |
| 1 | query-present | 321,337 | 320,718 | -0.19% | 321,750 | +0.13% |
| 1 | query-absent | 322,165 | 317,259 | -1.52% | 320,308 | -0.58% |
| 32 | incrby | 1,792,115 | 1,773,050 | -1.06% | 1,792,115 | +0.00% |
| 32 | query-present | 1,984,127 | 1,992,032 | +0.40% | 2,024,292 | +2.02% |
| 32 | query-absent | 2,024,292 | 2,040,816 | +0.82% | 2,024,292 | +0.00% |

## Reproduce

From the RedisBloom root, with redis-py installed and release builds of both modules:

```sh
python3 tests/benchmarks/bloom_seed_rps.py --filter CMS \
  --server /path/to/redis-server \
  --benchmark /path/to/redis-benchmark \
  --master /path/to/master/redisbloom.so \
  --branch /path/to/branch/redisbloom.so
```

Repeat with `--unix-socket`. JSON output includes every sample, server CPU time
per request, and median summaries. Both runs completed successfully, including
all total-count and configuration assertions.

## Raw RPS samples

Whole RPS, rounds 1–5, including every outlier.

| Transport | Pipeline | Operation | Mode | Five samples |
|---|---|---|---|---|
| TCP | 1 | incrby | master | 128766, 129099, 126839, 78827, 118427 |
| TCP | 1 | incrby | default | 130685, 127033, 73855, 81182, 122790 |
| TCP | 1 | incrby | random | 128634, 131027, 123762, 82740, 124969 |
| TCP | 1 | query-present | master | 62220, 70942, 126807, 128139, 74272 |
| TCP | 1 | query-present | default | 66845, 74239, 129769, 120627, 82386 |
| TCP | 1 | query-present | random | 75313, 74305, 71664, 128271, 73003 |
| TCP | 1 | query-absent | master | 127747, 122730, 71613, 69358, 68710 |
| TCP | 1 | query-absent | default | 70582, 124657, 77483, 71531, 125818 |
| TCP | 1 | query-absent | random | 75517, 128304, 122339, 70087, 111259 |
| TCP | 32 | incrby | master | 135318, 1322751, 1424501, 1466276, 1432665 |
| TCP | 32 | incrby | default | 1453488, 1449275, 1440922, 1453488, 142857 |
| TCP | 32 | incrby | random | 1404494, 1453488, 1428572, 1440922, 168350 |
| TCP | 32 | query-present | master | 1582278, 1592357, 1639344, 1672241, 1644737 |
| TCP | 32 | query-present | default | 1644737, 1633987, 1612903, 1618123, 1633987 |
| TCP | 32 | query-present | random | 1428572, 1618123, 1612903, 1587302, 1587302 |
| TCP | 32 | query-absent | master | 1623377, 1607717, 1655629, 1633987, 1623377 |
| TCP | 32 | query-absent | default | 1612903, 146156, 1628664, 1628664, 1672241 |
| TCP | 32 | query-absent | random | 1567398, 1633987, 1533742, 1474926, 1602564 |
| Unix socket | 1 | incrby | master | 320513, 320308, 320718, 316256, 104910 |
| Unix socket | 1 | incrby | default | 318471, 318269, 309789, 100462, 99960 |
| Unix socket | 1 | incrby | random | 311915, 320924, 315259, 103477, 101771 |
| Unix socket | 1 | query-present | master | 324675, 321337, 318066, 321750, 314465 |
| Unix socket | 1 | query-present | default | 318471, 322997, 320718, 320924, 317058 |
| Unix socket | 1 | query-present | random | 315657, 321750, 324886, 319285, 322165 |
| Unix socket | 1 | query-absent | master | 106428, 319898, 322373, 322165, 322165 |
| Unix socket | 1 | query-absent | default | 100523, 102775, 317259, 320924, 324886 |
| Unix socket | 1 | query-absent | random | 103306, 316456, 320308, 323206, 321543 |
| Unix socket | 32 | incrby | master | 1792115, 1792115, 1798561, 1818182, 1773050 |
| Unix socket | 32 | incrby | default | 1773050, 1742160, 1754386, 1805054, 1785714 |
| Unix socket | 32 | incrby | random | 1792115, 1779360, 1824818, 1798561, 1754386 |
| Unix socket | 32 | query-present | master | 1992032, 161917, 2016129, 1984127, 1937985 |
| Unix socket | 32 | query-present | default | 1984127, 1992032, 1992032, 2049180, 2032520 |
| Unix socket | 32 | query-present | random | 2040816, 139043, 2024292, 2040816, 2016129 |
| Unix socket | 32 | query-absent | master | 2040816, 2008032, 2032520, 2024292, 1976285 |
| Unix socket | 32 | query-absent | default | 1992032, 1992032, 2040816, 2074689, 2057613 |
| Unix socket | 32 | query-absent | random | 2032520, 2024292, 2008032, 2066116, 2016129 |

