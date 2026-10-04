"""Compare default, random, and boundary manual seeds on non-adversarial inputs.

Run with RLTest --test test_bloom_seed_accuracy. Dataset generation is deterministic;
random filter seeds are printed so individual trials can be replayed with SEED <value>.
Accuracy below means correct rejection of absent items (1 - false-positive rate),
not an application-wide accuracy that depends on the proportion of present items.
"""

import hashlib
import math
import os
import struct

from common import Env


def test_bloom_seed_accuracy():
    env = Env(decodeResponses=False)
    # Full statistical sweeps are opt-in; CI runs them once on regular Jammy x64.
    if os.getenv('SEED_ACCURACY') != '1':
        env.skip()
    queries = 100_000
    random_trials = 10
    capacity = 2_000
    batch_size = 5_000
    trials = [('default', [])] + [('random', ['SEED', 'random'])] * random_trials
    trials += [('manual-zero', ['SEED', '0']),
               ('manual-high-bit', ['SEED', '0x100000000']),
               ('manual-maximum', ['SEED', '0xffffffffffffffff'])]
    totals = {mode: [0, 0, 0, 0] for mode, _ in trials}
    datasets = {
        'sequential': lambda i: str(i).encode(),
        'common-prefix': lambda i: b'user:tenant:0000000000000000:' + str(i).encode(),
        'binary': lambda i: hashlib.sha256(str(i).encode()).digest(),
    }
    scenarios = [('half-full', capacity // 2, ['NONSCALING']),
                 ('full', capacity, ['NONSCALING']),
                 ('expanded', capacity * 7, ['EXPANSION', 2])]

    def count_positive(command, key, values):
        return sum(sum(env.cmd(command, key, *values[i:i + batch_size]))
                   for i in range(0, len(values), batch_size))

    print('\nBLOOM ACCURACY: target,dataset,load,default_fp,random_fp_mean,'
          'random_fp_min,random_fp_max,default_accuracy_pct,random_accuracy_pct,'
          'random_minus_default_pp', flush=True)
    for dataset, encode in datasets.items():
        inserted = [encode(i) for i in range(capacity * 7)]
        absent = [encode(i) for i in range(1_000_000, 1_000_000 + queries)]
        env.assertEqual(len(inserted), len(set(inserted)))
        env.assertEqual(queries, len(set(absent)))
        env.assertFalse(set(inserted).intersection(absent))
        for error in (0.01, 0.001, 0.0001):
            for scenario, count, options in scenarios:
                false_positives = []
                for mode, seed_option in trials:
                    key = 'seed-accuracy'
                    env.cmd('DEL', key)
                    env.cmd('BF.RESERVE', key, error, capacity, *options, *seed_option)
                    _, header = env.cmd('BF.SCANDUMP', key, 0)
                    seed = struct.unpack('=Q', header[-8:])[0]
                    if mode == 'default':
                        env.assertEqual(0xc6a4a7935bd1e995, seed)
                    count_positive('BF.MADD', key, inserted[:count])
                    expected_filters = 3 if scenario == 'expanded' else 1
                    env.assertEqual([expected_filters], env.cmd('BF.INFO', key, 'FILTERS'))
                    fn = count - count_positive('BF.MEXISTS', key, inserted[:count])
                    fp = count_positive('BF.MEXISTS', key, absent)
                    if mode in ('default', 'random'):
                        false_positives.append(fp)
                    else:
                        env.assertEqual(int(seed_option[1], 0), seed)
                    totals[mode][0] += fp
                    totals[mode][1] += queries
                    totals[mode][2] += fn
                    totals[mode][3] += count
                    context = f'{dataset}/{error}/{scenario}/{mode}/seed={seed}'
                    env.assertEqual(0, fn, message=context)
                    # Allow sampling noise above the configured bound, not an exact
                    # equality between seeds. Six sigma avoids a flaky multi-trial test.
                    limit = error + 6 * math.sqrt(error * (1 - error) / queries) + 1 / queries
                    env.assertLessEqual(fp / queries, limit, message=context)
                default = false_positives[0] / queries
                random = sum(false_positives[1:]) / (queries * random_trials)
                print(f'ACCURACY {error},{dataset},{scenario},{false_positives[0]},'
                      f'{sum(false_positives[1:]) / random_trials:.1f},'
                      f'{min(false_positives[1:])},{max(false_positives[1:])},'
                      f'{100 * (1 - default):.6f},{100 * (1 - random):.6f},'
                      f'{100 * (default - random):+.6f}', flush=True)
    env.cmd('DEL', 'seed-accuracy')
    for mode, (fp, negative_count, fn, positive_count) in totals.items():
        print(f'TOTAL {mode}: false_positives={fp}/{negative_count}, '
              f'false_positive_rate={100 * fp / negative_count:.6f}%, '
              f'absent_accuracy={100 * (1 - fp / negative_count):.6f}%, '
              f'false_negatives={fn}/{positive_count}', flush=True)
