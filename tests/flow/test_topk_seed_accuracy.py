"""Top-K recall against exact event counts, with optional real-master comparison.

Run with RLTest --test test_topk_seed_accuracy. Set TOPK_MASTER_MODULE to a
separately built master module for comparison.
The printed accuracy is true heavy hitters found / k, NOT exact count matches.
Recall comparisons are informational: seed-dependent collisions can help or hurt
these fixed datasets. Structural correctness assertions still fail the test.
"""

from collections import Counter, defaultdict
import hashlib
import json
import os
import random
import statistics

from common import Env
from rdb_corruption_utils import load_len


def test_topk_seed_accuracy():
    env = Env(decodeResponses=False)
    # Full statistical sweeps are opt-in; CI runs them once on regular Jammy x64.
    if os.getenv('SEED_ACCURACY') != '1':
        env.skip()
    env.skipOnCluster()
    trials = 20
    profiles = [(10, []), (10, [64, 5, 0.9]),
                (50, [64, 3, 0.8]), (50, [256, 7, 0.99])]
    cases = []
    for pattern, encode in (
        ('sequential', lambda i: str(i).encode()),
        ('common-prefix', lambda i: b'page:tenant:0000000000000000:' + str(i).encode()),
        # TOPK.LIST uses C strings on master too; avoid its existing NUL truncation.
        ('binary-no-nul', lambda i: hashlib.sha256(str(i).encode()).digest().replace(b'\0', b'\xff')),
    ):
        items = [encode(i) for i in range(1000)]
        env.assertEqual(1000, len(set(items)))
        for profile, (k, params) in enumerate(profiles):
            for workload in ('hot', 'zipf', 'near-cutoff', 'changing-leaders'):
                if workload == 'hot':
                    counts = [100 + k - i if i < k else 2 for i in range(1000)]
                elif workload == 'zipf':
                    counts = [max(1, 1500 // (i + 1)) for i in range(1000)]
                elif workload == 'near-cutoff':
                    counts = [20 + 2 * k - i if i < 2 * k else 2 for i in range(1000)]
                else:
                    counts = [50 if i < k else 80 if i < 2 * k else 2 for i in range(1000)]
                truth = Counter(dict(zip(items, counts)))
                cutoff = sorted(counts, reverse=True)[k - 1]
                mandatory = {item for item, count in truth.items() if count > cutoff}
                tied = {item for item, count in truth.items() if count == cutoff}
                cases.append((f'{pattern}/{profile}/{workload}', k, params, truth,
                              mandatory, tied, items[:k] if workload == 'changing-leaders' else []))

    results = defaultdict(list)
    baseline_module = os.getenv('TOPK_MASTER_MODULE')
    modes = [('master', baseline_module)] if baseline_module else []
    modes += [('default', None), ('random', None)]
    for mode, module in modes:
        # Separate processes: do not interleave modes through one global rand() state.
        env = Env(module=module, decodeResponses=False, freshEnv=True)
        for name, k, params, truth, mandatory, tied, early in cases:
            env.cmd('TOPK.RESERVE', 'template', k, *params)
            template = env.cmd('DUMP', 'template')
            env.cmd('DEL', 'template')
            for trial in range(trials):
                rng = random.Random(17000 + trial)
                # Replay the identical unit-event order in every mode. For a changing
                # stream, previous leaders arrive first, followed by the new leaders.
                first = [item for item in early for _ in range(truth[item])]
                rest = [item for item, count in truth.items() if item not in early
                        for _ in range(count)]
                rng.shuffle(first)
                rng.shuffle(rest)
                stream = first + rest
                env.assertEqual(truth, Counter(stream))
                options = ['SEED', 'random'] if mode == 'random' else []
                env.cmd('TOPK.RESERVE', 'accuracy', k, *params, *options)
                seed = 0
                if mode == 'random':
                    # Before insertion, only the trailing seed differs from the template.
                    payload = env.cmd('DUMP', 'accuracy')
                    env.assertEqual(template[:-13], payload[:len(template) - 13])
                    seed, _, end, _ = load_len(payload, len(template) - 12)
                    env.assertEqual(b'\x00', payload[end:-10])
                    env.assertLessEqual(seed, 0xffffffff)
                for start in range(0, len(stream), 1024):
                    env.cmd('TOPK.ADD', 'accuracy', *stream[start:start + 1024])
                listed = env.cmd('TOPK.LIST', 'accuracy')
                selected = set(listed)
                env.assertEqual(k, len(listed))
                env.assertEqual(k, len(selected))
                env.assertTrue(selected.issubset(truth))
                env.assertEqual([1] * k, env.cmd('TOPK.QUERY', 'accuracy', *listed))
                # Ties at rank k are equally valid; do not penalize arbitrary tie order.
                hits = len(selected & mandatory) + min(k - len(mandatory), len(selected & tied))
                accuracy = 100 * hits / k
                results[name, mode].append(accuracy)
                env.cmd('DEL', 'accuracy')
    for name, k, _, _, _, _, _ in cases:
        for mode, _ in modes:
            values = results[name, mode]
            print('TOPK_CASE ' + json.dumps(dict(case=name, mode=mode, k=k,
                  accuracy_pct=statistics.mean(values), min_pct=min(values), max_pct=max(values),
                  stdev_pp=statistics.stdev(values))), flush=True)
        comparisons = [('random', 'default')]
        if baseline_module:
            comparisons += [('default', 'master'), ('random', 'master')]
        for mode, reference in comparisons:
            differences = [a - b for a, b in zip(results[name, mode], results[name, reference])]
            delta = statistics.mean(differences)
            print('TOPK_DELTA ' + json.dumps(dict(case=name, mode=mode, reference=reference,
                  delta_pp=delta, paired_standard_error_pp=statistics.stdev(differences) / trials ** 0.5)), flush=True)
    for mode, _ in modes:
        values = [value for (name, measured_mode), scores in results.items()
                  if measured_mode == mode for value in scores]
        print('TOPK_TOTAL ' + json.dumps(dict(mode=mode, sketches=len(values),
              accuracy_pct=statistics.mean(values))), flush=True)
