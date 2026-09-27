# Bloom seed accuracy results

## Result

Measured on 2026-09-27 using the current MurmurHash implementation. The test passed in 32 seconds.

| Metric | Default seed | Random seeds |
| --- | ---: | ---: |
| Known-absent queries | 2,700,000 | 27,000,000 |
| False positives | 6,281 | 63,288 |
| False-positive rate | 0.232630% | 0.234400% |
| Accuracy on absent items | 99.767370% | 99.765600% |
| False negatives / present-item checks | 0 / 153,000 | 0 / 1,530,000 |

Random-minus-default absent-item accuracy: **-0.001770 percentage points** (about 18 additional false positives per million absent queries for this mix of configurations).

These are descriptive results for this run, not proof that either seed choice is universally better. The per-case difference goes in both directions. Changing the seed changes which items collide; it does not change the filter's capacity, bit budget, or number of hash probes. No hash-algorithm switch is involved.

## Coverage and method

- Default mode omits SEED and verifies the stored seed is 0xc6a4a7935bd1e995.
- Each case compares that default with 10 independently generated SEED random keys.
- 27 cases, 297 filters, 29.7 million absent queries and 1.683 million present-item checks.
- Requested false-positive rates: 1%, 0.1%, 0.01%.
- Initial capacity: 2,000.
- Half-full: 1,000 inserted items, NONSCALING.
- Full: 2,000 inserted items, NONSCALING.
- Expanded: 14,000 inserted items, EXPANSION 2; verifies three filters.
- Inputs: sequential decimal strings, strings sharing a long prefix, and SHA-256-derived binary values.
- Inserted values and 100,000 absent values per dataset are deterministic, unique, and disjoint. Every seed in a case receives identical inputs.
- Present-item queries happen after insertion and must have zero false negatives.
- Each trial's false-positive rate must not exceed the requested rate plus six binomial standard deviations and one query of rounding tolerance. This is a regression guard, not a confidence interval or a formal equivalence test.
- The test prints the actual seed and false-positive count for every trial, plus per-case and aggregate results.

Accuracy here is **1 - false-positive rate**, specifically accuracy on known-absent items. Present-item accuracy was 100% in this run. Overall application accuracy also depends on its mix of present and absent queries.

Aggregate rates weight all 27 configurations equally within each mode. They are not a promise of a single production error rate. Zero observed false positives in a row does not establish a true zero probability. Queries and datasets are reused across seeds, so the query total must not be interpreted as that many independent experiments.

## Per-case measurements

Each false-positive count is out of 100,000 absent queries. Random counts show the mean and observed minimum–maximum across 10 seeds. Delta is random-minus-default accuracy in percentage points.

| Target FPR | Input | Load | Default FP | Random FP mean (range) | Default accuracy | Random mean accuracy | Delta (pp) |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: |
| 1% | sequential | half-full | 21 | 28.0 (19–38) | 99.979000% | 99.972000% | -0.007000 |
| 1% | sequential | full | 1017 | 1010.9 (966–1054) | 98.983000% | 98.989100% | +0.006100 |
| 1% | sequential | expanded | 878 | 877.2 (844–932) | 99.122000% | 99.122800% | +0.000800 |
| 0.1% | sequential | half-full | 0 | 1.3 (0–3) | 100.000000% | 99.998700% | -0.001300 |
| 0.1% | sequential | full | 105 | 100.4 (88–123) | 99.895000% | 99.899600% | +0.004600 |
| 0.1% | sequential | expanded | 84 | 88.9 (76–98) | 99.916000% | 99.911100% | -0.004900 |
| 0.01% | sequential | half-full | 0 | 0.1 (0–1) | 100.000000% | 99.999900% | -0.000100 |
| 0.01% | sequential | full | 11 | 11.2 (6–17) | 99.989000% | 99.988800% | -0.000200 |
| 0.01% | sequential | expanded | 9 | 11.5 (4–20) | 99.991000% | 99.988500% | -0.002500 |
| 1% | common-prefix | half-full | 26 | 26.7 (17–38) | 99.974000% | 99.973300% | -0.000700 |
| 1% | common-prefix | full | 960 | 985.4 (896–1054) | 99.040000% | 99.014600% | -0.025400 |
| 1% | common-prefix | expanded | 913 | 862.4 (816–913) | 99.087000% | 99.137600% | +0.050600 |
| 0.1% | common-prefix | half-full | 0 | 0.6 (0–3) | 100.000000% | 99.999400% | -0.000600 |
| 0.1% | common-prefix | full | 101 | 101.9 (89–118) | 99.899000% | 99.898100% | -0.000900 |
| 0.1% | common-prefix | expanded | 91 | 93.8 (75–110) | 99.909000% | 99.906200% | -0.002800 |
| 0.01% | common-prefix | half-full | 0 | 0.1 (0–1) | 100.000000% | 99.999900% | -0.000100 |
| 0.01% | common-prefix | full | 6 | 11.6 (8–15) | 99.994000% | 99.988400% | -0.005600 |
| 0.01% | common-prefix | expanded | 10 | 11.1 (7–15) | 99.990000% | 99.988900% | -0.001100 |
| 1% | binary | half-full | 36 | 24.1 (17–33) | 99.964000% | 99.975900% | +0.011900 |
| 1% | binary | full | 955 | 994.5 (909–1053) | 99.045000% | 99.005500% | -0.039500 |
| 1% | binary | expanded | 854 | 881.2 (830–907) | 99.146000% | 99.118800% | -0.027200 |
| 0.1% | binary | half-full | 1 | 1.0 (0–2) | 99.999000% | 99.999000% | +0.000000 |
| 0.1% | binary | full | 102 | 96.0 (81–121) | 99.898000% | 99.904000% | +0.006000 |
| 0.1% | binary | expanded | 80 | 86.8 (62–105) | 99.920000% | 99.913200% | -0.006800 |
| 0.01% | binary | half-full | 0 | 0.4 (0–1) | 100.000000% | 99.999600% | -0.000400 |
| 0.01% | binary | full | 10 | 12.2 (9–16) | 99.990000% | 99.987800% | -0.002200 |
| 0.01% | binary | expanded | 11 | 9.5 (5–12) | 99.989000% | 99.990500% | +0.001500 |

## Re-run

From tests/flow, with RLTest installed and the module built:

```sh
python3 -m RLTest \
  --module ../../bin/macos-arm64v8-release/redisbloom.so \
  --oss-redis-path /path/to/redis-server \
  --randomize-ports --test test_bloom_seed_accuracy
```

Adjust the module path for the platform/build. Default results are deterministic for the same implementation and data; random-seed results naturally vary between runs.

This test does not measure attack resistance, secrecy of seeds, throughput, or XXH3. It also does not exhaust all possible seeds, filter sizes, or input distributions.

