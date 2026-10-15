# Context

The automated benchmark definitions included within `tests/benchmarks` folder, provides a framework for evaluating and comparing feature branches and catching regressions prior to letting them into the master branch.

To be able to run local benchmarks you need the pinned `redisbench-admin` version in `requirements.txt` [[tool repo for full details](https://github.com/RedisLabsModules/redisbench-admin)] and the benchmark tool specified on each configuration file . You can install redisbench-admin via PyPi as any other package.
```
pip3 install -r tests/benchmarks/requirements.txt
```

## Usage

- Local benchmarks: `make benchmark`
- Remote benchmarks:  `make benchmark REMOTE=1`


## Included benchmarks

Each benchmark requires a benchmark definition yaml file to present on the current directory. The benchmark spec file is fully explained on the following link: https://github.com/RedisLabsModules/redisbench-admin/tree/master/docs


## CI integration

CI benchmarks are triggered on:
- nightly
- pushes to branches named `master`
- version tags

## Paired benchmark comparisons

Event CI and Event Nightly run all 19 existing workloads as paired comparisons.
No workload is deleted or shortened. These comparisons measure throughput, not memory usage.
The existing legacy benchmark workflow remains available.

| Trigger | Baseline | Candidate | Regression policy |
| --- | --- | --- | --- |
| Non-draft, non-docs-only PR | Pinned PR target branch SHA | Pinned PR head SHA | Report only; execution/coverage errors still fail |
| Scheduled nightly on master | Pinned `benchmark-baseline` SHA | Pinned master SHA | Fail on any final degradation of 5% or more |
| Manual Event Nightly | `baseline_branch` input, default `benchmark-baseline` when blank | Selected branch's dispatch SHA | Same failure policy as nightly; never promotes |
| Other events | — | — | Comparison skipped |

Each module is built once from its pinned checkout with `make build`. Both builds use
the same absolute directory, separate build outputs, and the same compiler environment.
Five jobs partition the standalone YAMLs, then a report job validates and merges every
expected result. Event CI uses one shared Redis and client-tool build. AWS uses the
existing `redisbloom-m5` server/client topology per shard; both modules run sequentially on
that same topology. Remote module paths and SHA256 checks verify the loaded binary.

Each workload starts a fresh Redis server. On AWS, Redis and benchmark clients each use
two physical cores on their respective hosts, away from NIC interrupts and housekeeping;
SMT siblings are reserved too. Memtier and redis-benchmark are pinned with `taskset` wrappers.
Affinity setup or cleanup failures stop measurements rather than mixing environments.

A first-pair difference greater than 5% in either direction triggers three additional
paired attempts. The order alternates baseline/candidate and candidate/baseline.
The report shows each side's median throughput and the median of the paired percentage
changes. Smaller differences run once. Execution errors are reported, not retried.

- Green percentage: improvement.
- Yellow percentage: degradation below 5%.
- Red percentage: degradation of 5% or more, or an execution error.
- Orange benchmark name: retries were needed.

The initial AWS report publishes final throughput medians to the existing Grafana
RedisTimeSeries data source, using the legacy repository, workload, metric and `circleci`
labels. Baseline and candidate have separate branch/commit identities. All workloads publish operations/sec. Confirmation does not publish duplicate measurements.

### Baseline promotion

Only scheduled master runs may update **`benchmark-baseline`**, and only when:

1. Every standalone workload has valid results, with none degrading by 5% or more.
2. The geometric mean of all paired throughput changes is positive (no 5% overall
   improvement requirement). Each workload has equal weight.
3. A separate confirmation round on fresh AWS hosts, using the exact same builds and
   no retries, also meets those criteria with identical coverage and Redis version.

If baseline and candidate commits are identical, builds and comparisons are skipped.
Promotion uses a lease on the original baseline SHA: concurrent changes prevent the
push. No other branch is written. Manual runs never promote. Failed jobs can reuse
successful build/initial-round artifacts when rerun because artifact names use the run
ID rather than its attempt number. Cancelled runs do not start report jobs; AWS teardown
still runs.

Before enabling default nightly comparisons, initialize `benchmark-baseline` to the
chosen starting commit (for example, current master). The workflow deliberately fails
if the requested baseline branch does not exist; it does not silently pick another ref.
AWS and publishing use the existing `PERFORMANCE_EC2_*` and `PERFORMANCE_RTS_*` secrets,
the existing `PERFORMANCE_EC2_PRIVATE_PEM` SSH secret, and the existing Terraform infrastructure.
The new AWS jobs override the legacy Ubuntu 20.04 AMI with the shared us-east-2 Ubuntu
24.04 performance image (`ami-0ea6aaf5946625af3`) to match the build host’s glibc.
The legacy benchmark workflow is unchanged.
Repository branch rules must permit the scheduled workflow to update `benchmark-baseline`.

### Local comparison

Build both checkouts first. Install the pinned runner requirements and put `redis-server`,
`redis-benchmark` and `memtier_benchmark` on PATH. Dataset URLs are left unchanged.

```sh
python -m pip install -r tests/benchmarks/requirements.txt
python tests/benchmarks/compare_local.py \
  --baseline-module /absolute/path/to/baseline/redisbloom.so \
  --candidate-module /absolute/path/to/current/redisbloom.so \
  --output /absolute/path/to/new-results
```

Use `--benchmark NAME.yml` (repeatable) to select workloads and
`--report-only-regressions` to match Event CI's policy. The default local behavior fails
on a final degradation of 5% or more. Results include `comparison.json`, an HTML table
in `summary.md`, attempt histories, and runner logs. The timeout is per revision per
workload, defaulting to 1800 seconds; large datasets require sufficient memory/disk.
