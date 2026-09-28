# Cuckoo seed throughput versus master

Measured 2026-09-28 on Apple M4 Max, 36 GiB RAM, macOS.

## Conclusion

No consistent regression was demonstrated in the pipelined tests. TCP median
changes versus master were +1.88% to +3.54% for default and +1.56% to +4.55% for
random seeds. Unix-socket changes were -1.92% to +1.56% for default and -0.78% to
+1.96% for random seeds. These small differences do not establish either a real
improvement or the absence of a small regression.

Non-pipelined results are inconclusive. Both transports had large timing outliers
on master and branch. In particular, Unix-socket default-seed EXISTS-hit has a
-62.33% median RPS change, with three slow samples out of five; its server CPU
cost is 2.303 microseconds/request versus master's 2.292. This large wall-time
change is not matched by a comparable increase in CPU work. The cause of the
stalls was not established. Do not dismiss those results as a proven non-issue,
or use this desktop run as a performance release gate.

## Method

- Master: `a89aac8e75da4a1fbbb59a307ea2f3b4ae87870d` (freshly fetched origin/master).
- Branch: `24994c34dda38ddeba3f9d1c808a6bfae75d151e`.
- Both modules: macOS arm64 release builds; baseline Cuckoo sources/build definition checked against master.
- Same Redis server and redis-benchmark binaries as the Bloom comparison; Redis core revision `2c32610816636706b2203ef1a2b534a08d3a643f`.
- Reused `bloom_seed_rps.py` with `--filter CF`; no production changes.
- 16 clients; pipeline depths 1 and 32; 500,000 requests per sample; five rounds.
- Modes: master, branch with omitted seed (zero), branch with `SEED random`.
- Fresh isolated server per mode/round; persistence disabled; existing Redis instances untouched.
- Mode order rotates each round; benchmark workload RNG seed fixed at 12345.
- `CF.RESERVE key 10000000`, with default bucket size/iterations/expansion settings.
- ADD: fresh empty filter per sample; random items from a billion-item domain. CF.ADD stores duplicates too.
- EXISTS: 500,000 deterministic preloaded items; present queries use that domain, absent queries use a distinct prefix. Absent queries can still yield false positives.
- Prefill uses CF.INSERT; every prefill result must be 1. After every measured run, CF.INFO must show exactly one sub-filter and the expected insertion count.
- Initial capacity-1-million pilot aborted on master because the filter expanded. It supplied no recorded performance samples and is excluded from all tables. Capacity 10 million matches the repository's existing Cuckoo benchmark and isolates non-growing, lightly loaded filters; it is not a high-occupancy benchmark.
- 50,000 warm-up queries per server; reserve, prefill, warm-up and validation are outside measured RPS.
- RPS is commands/second, one item per command. All five samples are retained.
- 180 samples / 90 million measured commands across both transports, excluding the aborted pilot and warm-up/prefill.
- Not covered: CF.DEL, CF.ADDNX, measured CF.INSERT/CF.MEXISTS batches, high occupancy, expansion, explicit manual seeds separately, reserve/random-generation cost, persistence/replication load, long values, or other hardware.

## Median RPS

Negative change means slower than master. Percentages use unrounded values.

### TCP

| Pipeline | Operation | Master | Default | Change | Random | Change |
|---|---|---:|---:|---:|---:|---:|
| 1 | add | 70,215 | 83,375 | +18.74% | 130,719 | +86.17% |
| 1 | exists-hit | 73,142 | 73,110 | -0.04% | 71,429 | -2.34% |
| 1 | exists-miss | 123,762 | 128,932 | +4.18% | 131,996 | +6.65% |
| 32 | add | 1,533,742 | 1,587,302 | +3.49% | 1,557,632 | +1.56% |
| 32 | exists-hit | 1,552,795 | 1,607,717 | +3.54% | 1,623,377 | +4.55% |
| 32 | exists-miss | 1,538,462 | 1,567,398 | +1.88% | 1,577,287 | +2.52% |

### Unix socket

| Pipeline | Operation | Master | Default | Change | Random | Change |
|---|---|---:|---:|---:|---:|---:|
| 1 | add | 288,351 | 313,087 | +8.58% | 321,958 | +11.65% |
| 1 | exists-hit | 318,471 | 119,962 | -62.33% | 319,489 | +0.32% |
| 1 | exists-miss | 317,058 | 318,878 | +0.57% | 321,543 | +1.41% |
| 32 | add | 1,923,077 | 1,953,125 | +1.56% | 1,960,784 | +1.96% |
| 32 | exists-hit | 2,000,000 | 1,984,127 | -0.79% | 2,000,000 | +0.00% |
| 32 | exists-miss | 1,960,784 | 1,923,077 | -1.92% | 1,945,525 | -0.78% |

## Reproduce

From the RedisBloom root, with redis-py installed and matching release builds:

```sh
python3 tests/benchmarks/bloom_seed_rps.py --filter CF \
  --server /path/to/redis-server \
  --benchmark /path/to/redis-benchmark \
  --master /path/to/master/redisbloom.so \
  --branch /path/to/branch/redisbloom.so
```

Repeat with `--unix-socket`. The runner emits all samples, server CPU time per
request, and median summaries as JSON lines. Both capacity-10-million runs
completed successfully, including all insertion-count and single-filter checks.

## Raw RPS samples

Rounded values, rounds 1–5, including every timing outlier.

| Transport | Pipeline | Operation | Mode | Five samples |
|---|---|---|---|---|
| TCP | 1 | add | master | 122459, 67222, 64885, 70215, 142572 |
| TCP | 1 | add | default | 62205, 132661, 83375, 74383, 141884 |
| TCP | 1 | add | random | 128601, 130719, 76348, 139665, 137438 |
| TCP | 1 | exists-hit | master | 65833, 73142, 143678, 141643, 72046 |
| TCP | 1 | exists-hit | default | 69023, 73110, 120861, 145858, 71870 |
| TCP | 1 | exists-hit | random | 69118, 79936, 138581, 71429, 68222 |
| TCP | 1 | exists-miss | master | 126871, 123762, 61782, 72119, 135099 |
| TCP | 1 | exists-miss | default | 128932, 132066, 68729, 76982, 142410 |
| TCP | 1 | exists-miss | random | 131648, 131996, 74605, 142491, 139315 |
| TCP | 32 | add | master | 1543210, 1533742, 1538462, 1428572, 1501502 |
| TCP | 32 | add | default | 1538462, 1567398, 1612903, 1587302, 1597444 |
| TCP | 32 | add | random | 1577287, 1529052, 1557632, 133404, 1633987 |
| TCP | 32 | exists-hit | master | 1552795, 1592357, 1543210, 1453488, 1562500 |
| TCP | 32 | exists-hit | default | 1587302, 1607717, 1607717, 1694915, 1628664 |
| TCP | 32 | exists-hit | random | 1597444, 1602564, 1628664, 1644737, 1623377 |
| TCP | 32 | exists-miss | master | 1538462, 1572327, 1538462, 1529052, 1577287 |
| TCP | 32 | exists-miss | default | 1567398, 1533742, 1577287, 1672241, 134372 |
| TCP | 32 | exists-miss | random | 1577287, 1577287, 1547988, 1628664, 1644737 |
| Unix socket | 1 | add | master | 124502, 327439, 108956, 288351, 327011 |
| Unix socket | 1 | add | default | 325521, 309598, 108861, 313087, 323415 |
| Unix socket | 1 | add | random | 325098, 325945, 309598, 321958, 320513 |
| Unix socket | 1 | exists-hit | master | 318471, 326584, 318471, 114234, 327225 |
| Unix socket | 1 | exists-hit | default | 119962, 318674, 318269, 89718, 93440 |
| Unix socket | 1 | exists-hit | random | 319489, 322997, 322581, 94643, 103242 |
| Unix socket | 1 | exists-miss | master | 318878, 317058, 319285, 316857, 109433 |
| Unix socket | 1 | exists-miss | default | 313480, 113688, 318878, 324675, 323415 |
| Unix socket | 1 | exists-miss | random | 99463, 321543, 319898, 326371, 331345 |
| Unix socket | 32 | add | master | 1893939, 1953125, 1953125, 1923077, 1915709 |
| Unix socket | 32 | add | default | 1968504, 1953125, 1930502, 1968504, 1945525 |
| Unix socket | 32 | add | random | 1968504, 184298, 1945525, 1960784, 1992032 |
| Unix socket | 32 | exists-hit | master | 1908397, 1976285, 2000000, 2016129, 2016129 |
| Unix socket | 32 | exists-hit | default | 2024292, 1960784, 1945525, 1984127, 2024292 |
| Unix socket | 32 | exists-hit | random | 2000000, 1976285, 1736111, 2000000, 2008032 |
| Unix socket | 32 | exists-miss | master | 1879699, 1937985, 1968504, 1960784, 1984127 |
| Unix socket | 32 | exists-miss | default | 1992032, 1930502, 1901141, 1923077, 1923077 |
| Unix socket | 32 | exists-miss | random | 1953125, 1923077, 1879699, 1945525, 1976285 |

