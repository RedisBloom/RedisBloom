"""CMS frequency-estimation regression and collision diagnostics.

Set CMS_MASTER_MODULE to a separately built master module to run the real-master
comparison. Its accuracy percentage measures estimates within configured error,
not exact matches. The separate seed matrix retains exact-match diagnostics.
Both tests print random seeds for replay and do not test attack resistance.
"""

import hashlib
import json
import os

from common import Env
from rdb_corruption_utils import load_len


def cms_seed(payload):
    """Read the seed after the four scalar fields and cell-array RDB string."""
    _, _, pos, _ = load_len(payload, 1)  # Module ID.
    for _ in range(4):
        assert payload[pos] == 2  # Unsigned field.
        _, _, pos, _ = load_len(payload, pos + 1)
    assert payload[pos] == 5  # Cell-array string, possibly LZF compressed.
    length, encoded, pos, encoding = load_len(payload, pos + 1)
    if encoded:
        assert encoding == 3
        length, _, pos, _ = load_len(payload, pos)
        _, _, pos, _ = load_len(payload, pos)  # Uncompressed length.
    pos += length
    assert payload[pos] == 2
    seed, _, pos, _ = load_len(payload, pos + 1)
    assert payload[pos:-10] == b'\x00'  # Module EOF.
    return seed


def test_cms_seed_accuracy():
    env = Env(decodeResponses=False)
    queries, batch_size = 4096, 1024
    trials = [('default', [])] + [('random', ['SEED', 'random'])] * 10
    trials += [('manual-zero', ['SEED', 0]), ('manual-upper', ['SEED', '0x80000000']),
               ('manual-maximum', ['SEED', '0xffffffff'])]
    totals = {}
    datasets = {
        'sequential': lambda i: str(i).encode(),
        'common-prefix': lambda i: b'user:tenant:0000000000000000:' + str(i).encode(),
        'binary': lambda i: hashlib.sha256(str(i).encode()).digest(),
    }
    for dataset, encode in datasets.items():
        absent = [encode(i) for i in range(1_000_000, 1_000_000 + queries)]
        for width, depth in ((256, 3), (512, 5), (1024, 7)):
            for workload, count in (('empty', 0), ('sparse', width // 4), ('moderate', width),
                                    ('dense', width * 4), ('skewed', width * 4)):
                items = [encode(i) for i in range(count)]
                truth = [16 if i % 64 == 0 else 1 + i % 3 for i in range(count)]
                if workload != 'skewed':
                    truth = [1] * count
                env.assertEqual(count, len(set(items)))
                env.assertEqual(queries, len(set(absent)))
                env.assertFalse(set(items).intersection(absent))
                mass = sum(truth)
                for size in (1, 2, 4, 8):
                    default_results = {}
                    for mode, options in trials:
                        key = 'cms-seed-accuracy'
                        env.cmd('DEL', key)
                        # Exercise both public constructors with equivalent dimensions.
                        if size in (1, 4):
                            env.cmd('CMS.INITBYDIM', key, width, depth, 'CELL_SIZE', size, *options)
                        else:
                            env.cmd('CMS.INITBYPROB', key, 2 / width, 2 ** -depth,
                                    'CELL_SIZE', size, *options)
                        info = env.cmd('CMS.INFO', key)
                        info = dict(zip(info[::2], info[1::2]))
                        env.assertEqual(width, info[b'width'])
                        env.assertEqual(depth, info[b'depth'])
                        seed = cms_seed(env.cmd('DUMP', key))
                        if mode != 'random':
                            env.assertEqual(int(str(options[1]), 0) if options else 0, seed)
                        for start in range(0, count, batch_size):
                            pairs = zip(items[start:start + batch_size], truth[start:start + batch_size])
                            env.cmd('CMS.INCRBY', key, *(value for pair in pairs for value in pair))
                        for population, values, expected in (('present', items, truth),
                                                             ('absent', absent, [0] * queries)):
                            if not values:
                                continue
                            estimates = []
                            for start in range(0, len(values), batch_size):
                                estimates.extend(env.cmd('CMS.QUERY', key, *values[start:start + batch_size]))
                            errors = [estimate - actual for estimate, actual in zip(estimates, expected)]
                            context = f'{dataset}/{width}/{depth}/{workload}/{size}/{mode}/{seed}/{population}'
                            env.assertGreaterEqual(min(errors), 0, message=context)
                            env.assertLessEqual(max(estimates), mass, message=context)
                            if mode == 'default':
                                default_results[population] = estimates
                            elif mode == 'manual-zero':
                                env.assertEqual(default_results[population], estimates, message=context)
                            within = sum(error <= 2 * mass / width for error in errors)
                            # A deliberately loose empirical regression guard, not a
                            # confidence interval or proof of independent row hashes.
                            env.assertGreaterEqual(within / len(errors), 1 - max(0.1, 4 * 2 ** -depth),
                                                   message=context)
                            metrics = [len(errors), errors.count(0), sum(errors), within]
                            if mass:
                                total = totals.setdefault((mode, population), [0] * 4)
                                for i, value in enumerate(metrics):
                                    total[i] += value
    env.cmd('DEL', 'cms-seed-accuracy')
    for (mode, population), (count, exact, error_sum, within) in totals.items():
        print('CMS_TOTAL ' + json.dumps(dict(mode=mode, population=population, queries=count,
              exact=exact, exact_match_pct=100 * exact / count, mean_overestimate=error_sum / count,
              within_bound_pct=100 * within / count)), flush=True)


def test_cms_accuracy_against_master():
    """Compare real master/branch modules, not seed zero used as a master proxy.

    CMS_MASTER_MODULE must point to a separately built master module. Accuracy is
    the percentage with true_count <= estimate <= true_count + error * events.
    Default must match master exactly. Random-seed mean accuracy may drop by at
    most 0.1 percentage points PER workload/population, not merely in aggregate.
    This is an explicit regression budget, not a statistical significance test.
    """
    env = Env(decodeResponses=False)
    env.skipOnCluster()
    master_module = os.getenv('CMS_MASTER_MODULE')
    if not master_module:
        env.skip()
        return

    batch_size, random_trials, regression_budget_pp = 1024, 20, 0.1
    cases = []
    for pattern, encode in (
        ('sequential', lambda i: str(i).encode()),
        ('common-prefix', lambda i: b'page:tenant:0000000000000000:' + str(i).encode()),
        ('binary', lambda i: hashlib.sha256(str(i).encode()).digest()),
    ):
        for count in (1000, 10000):
            items = [encode(i) for i in range(count)]
            absent = [encode(1_000_000 + i) for i in range(4096)]
            for distribution in ('uniform', 'zipf', 'hot-pages'):
                truth = [10 if distribution == 'uniform' else
                         max(1, count // (i + 1)) if distribution == 'zipf' else
                         1000 if i < 10 else 1 for i in range(count)]
                for error, probability in ((0.01, 0.1), (0.01, 0.01), (0.001, 0.01)):
                    cases.append((f'{pattern}/{count}/{distribution}/{error}/{probability}',
                                  items, absent, truth, error, probability))

    def populate_and_query(server, case, options):
        _, items, absent, truth, error, probability = case
        server.cmd('DEL', 'cms-master-comparison')
        server.cmd('CMS.INITBYPROB', 'cms-master-comparison', error, probability, *options)
        for start in range(0, len(items), batch_size):
            pairs = zip(items[start:start + batch_size], truth[start:start + batch_size])
            server.cmd('CMS.INCRBY', 'cms-master-comparison',
                       *(value for pair in pairs for value in pair))
        values = items + absent
        return [estimate for start in range(0, len(values), batch_size)
                for estimate in server.cmd('CMS.QUERY', 'cms-master-comparison',
                                           *values[start:start + batch_size])]

    # RLTest switches/stops the previous environment when constructing another.
    # Capture real master results first, then run the branch on the same inputs.
    master = Env(module=master_module, decodeResponses=False, freshEnv=True)
    baseline = [populate_and_query(master, case, []) for case in cases]
    env = Env(decodeResponses=False, freshEnv=True)
    totals = {}
    for case, master_estimates in zip(cases, baseline):
        name, items, absent, truth, error, probability = case
        mass = sum(truth)
        measurements = {}
        trials = [('master', None), ('default', [])]
        trials += [('random', ['SEED', 'random'])] * random_trials
        for mode, options in trials:
            estimates = master_estimates if mode == 'master' else populate_and_query(env, case, options)
            seed = cms_seed(env.cmd('DUMP', 'cms-master-comparison')) if mode == 'random' else 0
            if mode == 'default':
                env.assertEqual(master_estimates, estimates, message=name)
            for population, actual, predicted in (
                ('present', truth, estimates[:len(items)]),
                ('absent', [0] * len(absent), estimates[len(items):]),
            ):
                errors = [estimate - real for estimate, real in zip(predicted, actual)]
                context = f'{name}/{mode}/{seed}/{population}'
                env.assertEqual(len(actual), len(predicted), message=context)
                env.assertGreaterEqual(min(errors), 0, message=context)
                within = sum(value <= error * mass for value in errors)
                accuracy = 100 * within / len(errors)
                normalized_error = 100 * sum(errors) / (len(errors) * mass)
                env.assertGreaterEqual(accuracy, 100 * (1 - probability), message=context)
                measurements.setdefault((mode, population), []).append((accuracy, normalized_error))
                total = totals.setdefault((mode, population), [0, 0, 0.0])
                total[0] += len(errors)
                total[1] += within
                total[2] += sum(errors) / mass
        for population in ('present', 'absent'):
            master_accuracy, master_error = measurements['master', population][0]
            random_accuracy = sum(x[0] for x in measurements['random', population]) / random_trials
            random_error = sum(x[1] for x in measurements['random', population]) / random_trials
            env.assertGreaterEqual(random_accuracy, master_accuracy - regression_budget_pp,
                                   message=f'{name}/{population}: random accuracy regression')
            # Also catch worsening error magnitude even if all estimates remain in bounds.
            env.assertLessEqual(random_error, master_error + 100 * error * 0.1,
                                message=f'{name}/{population}: mean error regression')
            print('CMS_MASTER_DELTA ' + json.dumps(dict(case=name, population=population,
                  accuracy_delta_pp=random_accuracy - master_accuracy,
                  mean_error_delta_pp=random_error - master_error)), flush=True)
    env.cmd('DEL', 'cms-master-comparison')
    for (mode, population), (queries, within, normalized_sum) in totals.items():
        print('CMS_MASTER_TOTAL ' + json.dumps(dict(mode=mode, population=population,
              queries=queries, within=within, accuracy_pct=100 * within / queries,
              mean_error_pct_of_events=100 * normalized_sum / queries)), flush=True)
