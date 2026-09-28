# CMS frequency-estimation regression against master

Measured 2026-09-28 against a separately built upstream RedisBloom master:
`a89aac8e75da4a1fbbb59a307ea2f3b4ae87870d`. Upstream master was fetched before
building; the older local branch named master was not used. Both modules ran on
the same Redis executable and received identical data. The working tree was not
switched or reset.

## Metric: estimates within the configured error allowance

CMS estimates event frequencies, not exact counts. The primary accuracy
percentage in this comparison is:

```
100 * queries satisfying (true count <= estimate <= true count + error * N)
    / total queries
```

`N` is the total number of events inserted, including repeated occurrences.
For example, with 100,000 events and error 0.001, the allowance is +100 counts.
If the true count is 1,000, estimates from 1,000 through 1,100 pass this check.
This is **not** a claim of 99.999% relative precision for each returned count.
It measures compliance with the chosen additive error allowance described in
the [Redis CMS sizing documentation](https://redis.io/docs/latest/develop/data-types/probabilistic/count-min-sketch/).

The earlier exact-match percentage was a collision diagnostic, not the right
headline metric for this regression question. It remains separately named
`exact_match_pct` in the broader seed matrix test.

## Results

Release-run percentage of estimates within the configured allowance:

| Query population | Actual master | Branch, default seed | Branch, random seeds |
| --- | ---: | ---: | ---: |
| Present items | 99.999327% | 99.999327% | 99.999495% |
| Never-inserted items | 99.999096% | 99.999096% | 99.999299% |

Master and the branch's default path returned **identical counts for all 518,184
queries**, not merely identical percentages. This is asserted query by query.

For random seeds, 30 of 5,940,000 present queries and 31 of 4,423,680 absent queries
exceeded the allowance. For master/default, 2 of 297,000 present and 2 of 221,184
absent queries exceeded it. There were **zero underestimates** in all modes.

The repeat with the branch under AddressSanitizer, using fresh random seeds,
returned 99.999596% present and 99.999344% absent. Master/default were unchanged.

### Error magnitude (also checked)

Mean overestimation, expressed as a percentage of the corresponding stream's
total event count (not of the individual item's true count):

| Population | Master/default | Random release | Random ASan |
| --- | ---: | ---: | ---: |
| Present | 0.171433% | 0.171844% | 0.171783% |
| Absent | 0.128990% | 0.129837% | 0.129685% |

Random mean error is slightly higher in these aggregates even though its
error-allowance success percentage is slightly higher too. Neither metric alone
should be represented as proof that random seeds always improve accuracy.

## Regression verdict and thresholds

**No regression detected under the explicit budgets below. Default results are
exactly unchanged; random-seed frequency estimation remains comparable to master
on these workloads.** This is not proof for every seed or adversarial stream.

The test fails if:

1. Any default-path estimate differs from actual master.
2. Any estimate is below the true count.
3. Any workload/trial/population has an error-allowance success rate below
   `100 * (1 - probability)` percent.
4. The mean across 20 random seeds loses more than **0.1 percentage points** of
   success rate versus master for any workload/population.
5. Random mean error increases by more than **10% of the configured additive
   error allowance** versus master for any workload/population.

These are explicit empirical regression budgets, not significance tests or
confidence intervals; query errors within a sketch can be correlated. No claim
is made that the configured probability guarantees a fraction for every fixed
stream and seed. Across cases, the worst observed random accuracy losses were
0.015 points (release) and 0.020 points (ASan), both below the 0.1-point budget.
Per-case checks prevent unrelated workloads from masking a failure in averages.

## Coverage

- 54 scenarios: three byte patterns x two cardinalities x three frequency
  distributions x three error/probability settings.
- Patterns: sequential IDs, common-prefix page IDs, SHA-256-derived binary IDs.
- Cardinalities: 1,000 and 10,000 distinct items; all present items plus 4,096
  distinct, never-inserted items queried in every trial.
- Event frequencies: uniform (10 per item), integer Zipf-style
  `max(1, cardinality // rank)`, and hot pages (first ten items occur 1,000 times,
  all other items once). Weighted CMS.INCRBY calls represent repeated events.
- CMS.INITBYPROB settings: (0.01, 0.1), (0.01, 0.01), (0.001, 0.01).
- Standard four-byte cells on both versions; the separate seed diagnostic test
  covers all cell sizes, manual boundaries, and both initialization commands.
- Every scenario runs on real master, branch default, and 20 fresh random seeds.
  No master estimates are simulated, inferred from seed zero, or hardcoded.
- 11,400,048 query evaluations per master-comparison run, **22,800,096 across
  release and ASan**. The two default runs repeat deterministic evidence.
- The ASan run additionally passed the broader seed/collision diagnostic test.
- Actual random seeds, per-case accuracy percentages/error magnitudes, deltas,
  and totals are printed as JSON for diagnosis and manual-seed replay.

## Run

Build the desired master revision separately first. From tests/flow:

```sh
CMS_MASTER_MODULE=/absolute/path/to/master/redisbloom.so \
CMS_MASTER_REVISION=<master-commit-sha> \
python3 -m RLTest --module ../../bin/macos-arm64v8-release/redisbloom.so \
  --oss-redis-path /path/to/redis-server --randomize-ports \
  --test test_cms_seed_accuracy:test_cms_accuracy_against_master
```

The comparison explicitly skips if CMS_MASTER_MODULE is absent; an ordinary
seed-matrix pass without that variable is **not** a master-regression result.
CMS_MASTER_REVISION labels the build supplied by the caller; it does not verify
binary provenance automatically. The measured baseline was built from the pinned
revision above in `/tmp/cms-master-accuracy.0mcVbW`.

Output prefixes: CMS_BASELINE, CMS_MASTER_TRIAL, CMS_MASTER_DELTA, CMS_MASTER_TOTAL.
Only tests/report changed; no production source changes or commits.
