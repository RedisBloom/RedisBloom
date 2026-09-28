# Bloom seed throughput versus master

Measured 2026-09-28 on Apple M4 Max, 36 GiB RAM, macOS.

## Conclusion

No consistent throughput regression was demonstrated across the two transports.
This is not proof of zero regression: the desktop was not idle, several runs had
multi-second stalls, and five short samples cannot establish a small effect reliably.
Do not treat this as a performance release gate.

Pipelined TCP: default changes ranged from -1.08% to +0.35%; random from -2.31%
to -0.35%. Pipelined Unix socket: default +0.83% to +4.57%; random -0.44% to +3.15%.
The direction of the ADD difference changed across transports.

Non-pipelined TCP medians are particularly unreliable: individual samples fall into
roughly 70k and 145k RPS groups on all three modes, producing apparent changes of
up to +82% and -50%. Unix-socket non-pipelined medians were much closer, but stalls
also occurred there. The cause of the stalls was not established; they cannot be
attributed specifically to TCP or to seed handling from this experiment.

## Method

- Master: `a89aac8e75da4a1fbbb59a307ea2f3b4ae87870d` (freshly fetched origin/master).
- Branch: `24994c34dda38ddeba3f9d1c808a6bfae75d151e`.
- Both modules: macOS arm64 release builds; baseline Bloom source/build definition checked against master.
- Same Redis server and redis-benchmark binaries, Redis core revision `2c32610816636706b2203ef1a2b534a08d3a643f`.
- 16 clients; pipeline depths 1 and 32; 500,000 requests per sample; five rounds.
- Three modes: master, branch with omitted seed, branch with `SEED random`.
- Fresh isolated Redis process per mode/round; persistence disabled; no existing server modified.
- Mode order rotates each round; fixed benchmark workload RNG seed 12345.
- `BF.RESERVE key 0.001 1000000`; no expansion is reached.
- ADD: fresh empty filter for each sample; random items from a one-billion-item domain (nearly all new).
- EXISTS: 500,000 deterministic preloaded items; present queries use that domain, absent queries use a different prefix. False positives remain possible for absent queries.
- 50,000-query warm-up per process. Prefill, reserve and warm-up are excluded from measured RPS.
- RPS is commands/second, one item per command. Medians retain every sample; no outliers discarded.
- 180 measured samples, 90 million commands across both transports.
- Not covered: reserve/random-generation cost, manual seeds separately, implicit creation, growth, batch commands, persistence/replication load, long values, or other hardware.

## Median RPS

Negative change means slower than master. Percentages use unrounded values.

### TCP

| Pipeline | Operation | Master | Default | Change | Random | Change |
|---|---|---:|---:|---:|---:|---:|
| 1 | add | 77,471 | 141,084 | +82.11% | 73,910 | -4.60% |
| 1 | exists-hit | 145,858 | 75,019 | -48.57% | 73,292 | -49.75% |
| 1 | exists-miss | 141,123 | 143,184 | +1.46% | 132,943 | -5.80% |
| 32 | add | 1,689,189 | 1,689,189 | +0.00% | 1,650,165 | -2.31% |
| 32 | exists-hit | 1,824,818 | 1,805,054 | -1.08% | 1,811,594 | -0.72% |
| 32 | exists-miss | 1,766,785 | 1,773,050 | +0.35% | 1,760,563 | -0.35% |

### Unix socket

| Pipeline | Operation | Master | Default | Change | Random | Change |
|---|---|---:|---:|---:|---:|---:|
| 1 | add | 337,610 | 338,753 | +0.34% | 335,121 | -0.74% |
| 1 | exists-hit | 339,213 | 340,599 | +0.41% | 340,599 | +0.41% |
| 1 | exists-miss | 338,983 | 330,251 | -2.58% | 338,753 | -0.07% |
| 32 | add | 2,066,116 | 2,083,333 | +0.83% | 2,118,644 | +2.54% |
| 32 | exists-hit | 2,183,406 | 2,283,105 | +4.57% | 2,252,252 | +3.15% |
| 32 | exists-miss | 2,232,143 | 2,252,252 | +0.90% | 2,222,222 | -0.44% |

## Reproduce

Requires the already-used redis-py dependency and release builds of both modules.
Run from the RedisBloom repository root. Substitute the paths to your binaries:

```sh
python3 tests/benchmarks/bloom_seed_rps.py \
  --server /path/to/redis-server \
  --benchmark /path/to/redis-benchmark \
  --master /path/to/master/redisbloom.so \
  --branch /path/to/branch/redisbloom.so
```

Repeat with `--unix-socket` for the second transport. The runner emits JSON lines
containing every sample, server CPU microseconds per request, and median summaries.
Both runs completed successfully. No production code was changed.

## Raw RPS samples

Values rounded to whole requests/second, listed by round 1–5. These include all stalls.

| Transport | Pipeline | Operation | Mode | Five samples |
|---|---|---|---|---|
| TCP | 1 | add | master | 123153, 77471, 145985, 74030, 75008 |
| TCP | 1 | add | default | 141084, 73164, 143102, 74162, 147232 |
| TCP | 1 | add | random | 70992, 73910, 120656, 146585, 73057 |
| TCP | 1 | exists-hit | master | 64425, 146800, 74140, 147667, 145858 |
| TCP | 1 | exists-hit | default | 69764, 139509, 75019, 141603, 72527 |
| TCP | 1 | exists-hit | random | 73292, 141965, 65894, 73035, 75245 |
| TCP | 1 | exists-miss | master | 141123, 142369, 147623, 72202, 74074 |
| TCP | 1 | exists-miss | default | 143184, 70872, 148236, 75426, 147362 |
| TCP | 1 | exists-miss | random | 132943, 72971, 70067, 148500, 144509 |
| TCP | 32 | add | master | 1724138, 150240, 1689189, 1639344, 1724138 |
| TCP | 32 | add | default | 1689189, 1655629, 1694915, 1712329, 1661130 |
| TCP | 32 | add | random | 1592357, 1700680, 1655629, 1650165, 1644737 |
| TCP | 32 | exists-hit | master | 1824818, 1798561, 1851852, 1779360, 1838235 |
| TCP | 32 | exists-hit | default | 1805054, 1818182, 1779360, 1805054, 1798561 |
| TCP | 32 | exists-hit | random | 1766785, 1838235, 1811594, 136537, 1811594 |
| TCP | 32 | exists-miss | master | 144383, 1766785, 1818182, 1742160, 1773050 |
| TCP | 32 | exists-miss | default | 1811594, 1858736, 1773050, 1773050, 1736111 |
| TCP | 32 | exists-miss | random | 1805054, 1779360, 1736111, 1760563, 1760563 |
| Unix socket | 1 | add | master | 99404, 335345, 341764, 337610, 339674 |
| Unix socket | 1 | add | default | 338524, 339674, 105307, 338753, 339213 |
| Unix socket | 1 | add | random | 330907, 335121, 339213, 340599, 332447 |
| Unix socket | 1 | exists-hit | master | 337610, 341530, 339905, 100482, 339213 |
| Unix socket | 1 | exists-hit | default | 102124, 340599, 342466, 105731, 341764 |
| Unix socket | 1 | exists-hit | random | 102522, 344116, 340599, 341997, 337610 |
| Unix socket | 1 | exists-miss | master | 335570, 339674, 338983, 338983, 339213 |
| Unix socket | 1 | exists-miss | default | 330251, 97012, 330251, 341064, 104297 |
| Unix socket | 1 | exists-miss | random | 339443, 110889, 339905, 103072, 338753 |
| Unix socket | 32 | add | master | 2118644, 145222, 2100840, 2066116, 2066116 |
| Unix socket | 32 | add | default | 2083333, 2016129, 2127660, 2074689, 2083333 |
| Unix socket | 32 | add | random | 2032520, 2118644, 2127660, 2118644, 2074689 |
| Unix socket | 32 | exists-hit | master | 2183406, 2136752, 2283105, 2252252, 2118644 |
| Unix socket | 32 | exists-hit | default | 2283105, 2293578, 2283105, 2262444, 2242153 |
| Unix socket | 32 | exists-hit | random | 2212390, 2252252, 2336449, 2293578, 138966 |
| Unix socket | 32 | exists-miss | master | 2232143, 2192983, 2242153, 2262444, 2192983 |
| Unix socket | 32 | exists-miss | default | 2252252, 2192983, 2272727, 2232143, 2272727 |
| Unix socket | 32 | exists-miss | random | 2192983, 2252252, 2222222, 2283105, 2202643 |

