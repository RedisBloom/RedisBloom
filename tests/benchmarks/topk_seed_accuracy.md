# TopK seed accuracy against master

Measured 2026-09-28 against the separately built RedisBloom master revision
`a89aac8e75da4a1fbbb59a307ea2f3b4ae87870d`, also used for the CMS comparison.
No production code changed for this test.

## What the percentage measures

Accuracy is top-K recall: **100 * correctly identified true top-K items / K**.
For K=10, finding nine of the actual ten most frequent items scores 90%.
Every stream's real frequencies are computed independently with Python Counter.
This tests TopK's purpose, not whether its estimated counts are exactly correct.
Because exactly K distinct items must be returned, precision equals recall here.
Items tied at the true cutoff receive interchangeable credit; list order does
not affect the score.

The aggregate is an equally weighted mean of scenario/trial percentages, so
K=50 cases do not receive five times the weight of K=10 cases.

## Result: small aggregate difference, but regression checks failed

| Completed run | Master | Branch default seed | Branch random seeds | Random minus default |
| --- | ---: | ---: | ---: | ---: |
| Release 1 | 85.916667% | 85.787500% | 85.658333% | -0.129167 pp |
| ASan | 85.764583% | 85.710417% | 85.610417% | -0.100000 pp |
| Release 2 | 85.800000% | 85.675000% | 85.554167% | -0.120833 pp |

Random seeds were slightly worse overall in all three runs. That does **not**
establish a universal regression: individual workloads move in both directions.
It also does **not** justify claiming no regression, because some per-case
differences exceeded the test's preselected five-percentage-point budget.

Release 2 by workload, averaging all byte patterns and parameter profiles:

| Workload | Default | Random | Difference |
| --- | ---: | ---: | ---: |
| Clear frequent leaders | 99.158333% | 99.083333% | -0.075000 pp |
| Zipf-style frequencies | 92.525000% | 91.858333% | -0.666667 pp |
| Close counts at cutoff | 85.733333% | 84.100000% | -1.633333 pp |
| Changing leaders | 65.283333% | 67.175000% | +1.891667 pp |

Specific flagged comparisons:

- Release 1: sequential / width 64 / K=10 / close cutoff: master 93.5%,
  default 91.0%, random 88.0%. Random versus master: **-5.5 pp**.
- ASan: the same case was master 94.5%, default 93.0%, random 89.0%.
  Random versus master: **-5.5 pp**. Common-prefix / default dimensions / close
  cutoff also lost **5.5 pp against the branch default** (59.5% to 54.0%).
- Release 2: sequential / default dimensions / close cutoff: master 64.5%,
  default 59.5%, random 55.0%. Random versus master: **-9.5 pp**. Common-prefix /
  default dimensions / close cutoff lost **5.5 pp against master** (59.0% to 53.5%).

All three completed runs failed accuracy assertions. The ASan run reported no
AddressSanitizer memory error; it must not be described as an overall test pass.
The five-point threshold was not relaxed after observing failures.

## Randomness and interpretation

Default hash seeds are still row numbers and 1919 (base seed 0). Random mode
changes the hash mapping, but both modes also use TopK's existing global rand()
for counter decay. The test does not reset that internal generator. Therefore,
even master and branch default can differ on identical input orders without a
hashing regression. Each mode uses a separate Redis process and twenty trials.

The input order for trial n is identical across modes, using Python shuffle seed
17000+n. Changing-leader streams deliver old leaders first and new leaders later.
Actual random hash seeds are printed, but replaying a hash seed alone cannot
reproduce internal decay decisions. Repeated runs reuse the twenty input orders
with fresh process/decay state and fresh random hash seeds.

The test prints paired standard errors as a variability diagnostic. It does not
perform a multiple-comparison significance test or prove that a threshold failure
is a code defect. Near-cutoff workloads consistently deserve closer investigation;
these results alone cannot separate every effect of hash choice and decay noise.
No adversarial-input security claim or throughput claim is made.

## Coverage

- 48 scenarios: three input patterns x four parameter profiles x four workloads.
- Patterns: sequential IDs, long shared-prefix IDs, and SHA-256-derived NUL-free
  binary IDs. Uniqueness is asserted.
- Profiles: K=10 with omitted parameters (width 8, depth 7, decay 0.9);
  K=10/width 64/depth 5/decay 0.9; K=50/width 64/depth 3/decay 0.8;
  K=50/width 256/depth 7/decay 0.99.
- 1,000 distinct items per stream; true counts vary by workload. Clear leaders
  have counts 100+K-i (zero-based i), Zipf-style counts are max(1, floor(1500/rank)), close
  cutoff has 2K near-ranked leaders, and changing leaders have counts 50 then 80.
- Actual unit events are passed to TOPK.ADD in batches, not grouped weighted
  inserts that would change ordering and decay behavior.
- 20 trials for master, default, and random in every scenario: 2,880 sketches and
  20,022,480 event insertions per run. **8,640 sketches and 60,067,440 events**
  across the three completed runs; the aborted NUL-input exploratory run is excluded.
- Assertions check K distinct returned items, membership in the original stream,
  TOPK.QUERY consistency, and mean recall regression budgets per scenario.
- Final test compares random/default, default/master, and random/master. Release 1
  preceded the direct random/default assertion; those rates were still recorded.
- Existing lifecycle/unit tests cover seed boundaries, empty items, persistence,
  and malformed inputs; this test concentrates on heavy-hitter identification.

Known limitation: master and branch TOPK.LIST return C strings and truncate at
embedded NUL. NUL-free binary inputs avoid measuring that unrelated existing
limitation as a seed regression. No production fix was bundled into this work.

## Run

From tests/flow:

```sh
TOPK_MASTER_MODULE=/absolute/path/to/master/redisbloom.so \
TOPK_MASTER_REVISION=<master-commit-sha> \
python3 -m RLTest --module ../../bin/macos-arm64v8-release/redisbloom.so \
  --oss-redis-path /path/to/redis-server --randomize-ports \
  --test test_topk_seed_accuracy
```

Without TOPK_MASTER_MODULE, the test compares default and random only and
explicitly reports that master was not tested. The revision variable is a label,
not automatic verification of a supplied binary's provenance.
JSON prefixes: TOPK_BASELINE, TOPK_TRIAL, TOPK_CASE, TOPK_DELTA, TOPK_TOTAL.
