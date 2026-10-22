"""Default versus random seed accuracy on deterministic, non-adversarial inputs.

Run with RLTest --test test_cuckoo_seed_accuracy. Accuracy means correct rejection
of known-absent items (1 - FPR), not workload-dependent overall accuracy. Printed
seeds allow individual trials to be replayed. This does not test attack resistance.
"""

import hashlib
import math
import os
import struct

from common import Env


def test_cuckoo_seed_accuracy():
    env = Env(decodeResponses=False)
    # Full statistical sweeps are opt-in; CI runs them once on regular Jammy x64.
    if os.getenv('SEED_ACCURACY') != '1':
        env.skip()
    capacity, queries, random_trials, batch_size = 2048, 100_000, 10, 5000
    trials = [('default', [])] + [('random', ['SEED', 'random'])] * random_trials
    trials += [('manual-zero', ['SEED', 0]),
               ('manual-upper', ['SEED', '0x100000000']),
               ('manual-maximum', ['SEED', '0xffffffffffffffff'])]
    totals = {mode: [0, 0, 0, 0] for mode, _ in trials}
    datasets = {
        'sequential': lambda i: str(i).encode(),
        'common-prefix': lambda i: b'user:tenant:0000000000000000:' + str(i).encode(),
        'binary': lambda i: hashlib.sha256(str(i).encode()).digest(),
    }
    scenarios = [('empty', 0), ('half', capacity // 2),
                 ('nominal-90pct', capacity * 9 // 10), ('expanded', capacity * 7)]

    def count_positive(key, values):
        return sum(sum(env.cmd('CF.MEXISTS', key, *values[i:i + batch_size]))
                   for i in range(0, len(values), batch_size))

    print('\nCUCKOO ACCURACY: dataset,bucket_size,load,stage,default_accuracy_pct,'
          'random_accuracy_pct,random_minus_default_pp,random_fp_min,random_fp_max', flush=True)
    for dataset, encode in datasets.items():
        inserted = [encode(i) for i in range(capacity * 7)]
        absent = [encode(i) for i in range(1_000_000, 1_000_000 + queries)]
        env.assertEqual(len(inserted), len(set(inserted)))
        env.assertEqual(queries, len(set(absent)))
        env.assertFalse(set(inserted).intersection(absent))
        for bucket_size in (1, 2, 4):
            for scenario, count in scenarios:
                measurements = {'inserted': [], 'deleted-compacted': []}
                for mode, seed_option in trials:
                    key = 'cuckoo-seed-accuracy'
                    env.cmd('DEL', key)
                    env.cmd('CF.RESERVE', key, capacity, 'BUCKETSIZE', bucket_size,
                            'MAXITERATIONS', 500, 'EXPANSION', 2, *seed_option)
                    _, header = env.cmd('CF.SCANDUMP', key, 0)
                    seed = struct.unpack_from('=Q', header, 38)[0] if header else 0
                    if mode != 'random':
                        expected = int(str(seed_option[1]), 0) if seed_option else 0
                        env.assertEqual(expected, seed)
                    for i in range(0, count, batch_size):
                        batch = inserted[i:min(i + batch_size, count)]
                        env.assertEqual([1] * len(batch), env.cmd('CF.INSERT', key, 'ITEMS', *batch))
                    stages = [('inserted', inserted[:count])]
                    if count:
                        stages.append(('deleted-compacted', inserted[count // 2:count]))
                    for stage, present in stages:
                        if stage == 'deleted-compacted':
                            # Delete only items actually inserted, once each. Never delete
                            # a false positive: doing so can legitimately cause false negatives.
                            pipe = env.getConnection().pipeline(transaction=False)
                            for item in inserted[:count // 2]:
                                pipe.execute_command('CF.DEL', key, item)
                            env.assertEqual([1] * (count // 2), pipe.execute())
                            env.assertEqual(b'OK', env.cmd('CF.COMPACT', key))
                        info = env.cmd('CF.INFO', key)
                        filters = dict(zip(info[::2], info[1::2]))[b'Number of filters']
                        if scenario == 'expanded' and stage == 'inserted':
                            env.assertGreater(filters, 1)
                        fn = len(present) - count_positive(key, present)
                        fp = count_positive(key, absent)
                        context = f'{dataset}/{bucket_size}/{scenario}/{stage}/{mode}/seed={seed}'
                        env.assertEqual(0, fn, message=context)
                        if not count:
                            env.assertEqual(0, fp, message=context)
                        # Loose full-occupancy union bound: two buckets per subfilter,
                        # bucket_size slots each, 255 nonzero fingerprints. Sampling
                        # tolerance is a regression guard, not proof of seed equivalence.
                        bound = min(1.0, 2 * bucket_size * filters / 255)
                        limit = bound + 6 * math.sqrt(bound * (1 - bound) / queries) + 1 / queries
                        env.assertLessEqual(fp / queries, limit, message=context)
                        if mode == 'manual-zero':
                            env.assertEqual(measurements[stage][0], fp, message=context)
                        for i, value in enumerate((fp, queries, fn, len(present))):
                            totals[mode][i] += value
                        if mode in ('default', 'random'):
                            measurements[stage].append(fp)
                for stage, counts in measurements.items():
                    if not counts:
                        continue
                    default = counts[0] / queries
                    random = sum(counts[1:]) / (queries * random_trials)
                    print(f'ACCURACY {dataset},{bucket_size},{scenario},{stage},'
                          f'{100 * (1 - default):.6f},{100 * (1 - random):.6f},'
                          f'{100 * (default - random):+.6f},{min(counts[1:])},'
                          f'{max(counts[1:])}', flush=True)
    env.cmd('DEL', 'cuckoo-seed-accuracy')
    for mode, (fp, negatives, fn, positives) in totals.items():
        print(f'TOTAL {mode}: false_positives={fp}/{negatives}, '
              f'false_positive_rate={100 * fp / negatives:.6f}%, '
              f'absent_accuracy={100 * (1 - fp / negatives):.6f}%, '
              f'false_negatives={fn}/{positives}', flush=True)
