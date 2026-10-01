# Cuckoo seed accuracy results

Measured on 2026-09-27 with the current MurmurHash implementation. Both runs passed.
This is a Cuckoo comparison; the existing Bloom results remain in bloom_seed_accuracy.md.

## Result

Accuracy here means correctly rejecting known-absent items, or 1 minus the false-positive
rate. It is not an application-wide accuracy score.

| Metric | Default seed 0 | Random seeds (run 2) |
| --- | ---: | ---: |
| Absent queries, including empty-filter checks | 6,300,000 | 63,000,000 |
| False positives | 111,886 | 1,118,163 |
| False-positive rate | 1.775968% | 1.774862% |
| Absent-item accuracy | 98.224032% | 98.225138% |
| False negatives / present checks | 0 / 232,245 | 0 / 2,322,450 |

Random-minus-default accuracy was **+0.001106 percentage points** in run 2
(about 11 fewer false positives per million absent queries for this particular mix).

Run 1 used fresh, different random seeds: 1,117,631 false positives out of
63,000,000 queries, accuracy **98.225983%**, delta **+0.001951 percentage points**.
Default results were identical in both runs. These are two batches of random seeds,
not two independent repetitions of the deterministic default experiment.

Excluding the empty-filter checks, run 2 accuracy was **97.928037% default**
versus **97.929328% random** (+0.001291 percentage points).
Empty filters had zero false positives in every trial.

The aggregate difference is small, but individual cases go in both directions.
There is no observed aggregate accuracy regression in these two runs; this is
not proof of equivalence or a guarantee that random seeds always improve accuracy.
A seed changes collisions and can affect when expansion occurs. Bucket size,
occupancy, and the number of subfilters are important to interpreting these rates.

## Coverage

- 36 configurations: three input patterns × three bucket sizes × four loads.
- Each configuration: default (SEED omitted), 10 independently generated random
  seeds, and explicit manual seeds 0, 0x100000000, and UINT64_MAX.
- 504 filters per run. The actual seed, subfilter count, false positives, and
  false negatives are printed for every measurement, allowing manual-seed replay.
- Initial capacity 2,048; bucket sizes 1, 2, 4; MAXITERATIONS 500; EXPANSION 2.
- Loads: empty, 1,024 items, 1,843 items (90% of nominal initial capacity), and
  14,336 items (7× initial capacity).
- All inserts must succeed. Expanded cases must have multiple subfilters.
  Nominal 90% is not a claim about actual occupancy: insertion collisions may
  trigger expansion earlier, especially with small buckets.
- Inputs: sequential decimal strings, long common-prefix strings, SHA-256-derived
  binary strings. Inserted and absent sets are deterministic, unique, and disjoint.
- 100,000 absent queries per measurement; every retained item checked for false negatives.
- Populated filters checked both immediately after insertion and after deleting
  the first half of inserted items once each, then compacting.
- Deleted items are NOT counted as known-absent queries: residual fingerprint
  collisions make that a different measurement. No never-inserted item is deleted.
- Per run: 88.2 million absent queries and 3,251,430 present checks across all modes.
  Across both runs: 176.4 million absent queries and 6,502,860 present checks;
  **zero false negatives**.
- Manual seed 0 reproduces default false-positive counts. Run 2 explicitly asserts
  this for every measurement. Upper-bit and maximum seeds also pass all checks.

The regression guard requires zero false negatives and checks a loose
full-occupancy union bound, 2 × bucket size × subfilter count / 255, capped at 1,
plus six binomial standard deviations and one query of tolerance. This bound is
based on approximately uniform fingerprints for these non-adversarial inputs;
it is not a guaranteed bound for malicious inputs or a formal equivalence test.
The 255 comes from the implementation's nonzero one-byte fingerprint.

## Breakdown (run 2)

Each row averages the three input patterns. Random results additionally average
10 seeds per pattern. Delta is random-minus-default accuracy in percentage points.
All empty cases are omitted below; they had 100% absent-item accuracy.

| Bucket size | Load | Stage | Default accuracy | Random mean accuracy | Delta (pp) |
| ---: | --- | --- | ---: | ---: | ---: |
| 1 | half | inserted | 99.621000% | 99.614433% | -0.006567 |
| 1 | half | deleted-compacted | 99.809333% | 99.799667% | -0.009667 |
| 1 | nominal-90pct | inserted | 99.455333% | 99.448100% | -0.007233 |
| 1 | nominal-90pct | deleted-compacted | 99.658667% | 99.657033% | -0.001633 |
| 1 | expanded | inserted | 98.172333% | 98.143600% | -0.028733 |
| 1 | expanded | deleted-compacted | 98.348667% | 98.411700% | 0.063033 |
| 2 | half | inserted | 99.238333% | 99.224800% | -0.013533 |
| 2 | half | deleted-compacted | 99.619667% | 99.609867% | -0.009800 |
| 2 | nominal-90pct | inserted | 98.731667% | 98.750900% | 0.019233 |
| 2 | nominal-90pct | deleted-compacted | 99.316667% | 99.301100% | -0.015567 |
| 2 | expanded | inserted | 95.809000% | 95.878867% | 0.069867 |
| 2 | expanded | deleted-compacted | 96.728000% | 96.772367% | 0.044367 |
| 4 | half | inserted | 98.454333% | 98.436467% | -0.017867 |
| 4 | half | deleted-compacted | 99.227000% | 99.218100% | -0.008900 |
| 4 | nominal-90pct | inserted | 97.251000% | 97.200000% | -0.051000 |
| 4 | nominal-90pct | deleted-compacted | 98.628667% | 98.589333% | -0.039333 |
| 4 | expanded | inserted | 91.127000% | 91.090667% | -0.036333 |
| 4 | expanded | deleted-compacted | 93.508000% | 93.580900% | 0.072900 |

## Manual boundary seeds (run 2)

| Seed | False positives / absent queries | Absent-item accuracy | False negatives |
| --- | ---: | ---: | ---: |
| 0 | 111,886 / 6,300,000 | 98.224032% | 0 |
| 0x100000000 | 113,239 / 6,300,000 | 98.202556% | 0 |
| UINT64_MAX | 112,804 / 6,300,000 | 98.209460% | 0 |

## Re-run

From tests/flow, after building the module:

```sh
python3 -m RLTest \
  --module ../../bin/macos-arm64v8-release/redisbloom.so \
  --oss-redis-path /path/to/redis-server \
  --randomize-ports --test test_cuckoo_seed_accuracy
```

Adjust paths for the platform. Runs took 109 and 105 seconds on this host while
partially overlapping; these durations are not throughput benchmarks.

These rates weight the tested configurations and stages equally within each mode.
They are not a production error-rate promise. Datasets are reused across seeds,
and pre/post-deletion queries share filter history; query totals are not counts of
independent statistical experiments. Random-seed results naturally vary per run.

This test does not establish resistance to accuracy attacks, seed secrecy, performance,
XXH3 behavior, or correctness for every possible seed, size, and input distribution.
No production implementation changes or fixture files were needed.
