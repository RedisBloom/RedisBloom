import tempfile

from common import Env
from test_bloom_seed import wait_until


CONFIG = 'bf.default-seed-policy'
CREATORS = [
    ('BF.RESERVE', [0.01, 100]),
    ('CF.RESERVE', [100]),
    ('CMS.INITBYDIM', [64, 5]),
    ('CMS.INITBYPROB', [0.03125, 0.03125]),
    ('TOPK.RESERVE', [3]),
    ('TOPK.RESERVE', [3, 64, 5, 0.9]),
]
AUTO_CREATORS = [
    ('BF.ADD', ['item']),
    ('BF.MADD', ['item', 'other']),
    ('BF.INSERT', ['CAPACITY', 50, 'ERROR', 0.02, 'NONSCALING', 'ITEMS', 'item']),
    ('CF.ADD', ['item']),
    ('CF.ADDNX', ['item']),
    ('CF.INSERT', ['CAPACITY', 50, 'ITEMS', 'item']),
    ('CF.INSERTNX', ['CAPACITY', 50, 'ITEMS', 'item']),
]


def test_seed_policy_creation():
    env = Env(decodeResponses=False)
    env.skipOnCluster()
    try:
        for i, (command, args) in enumerate(CREATORS + AUTO_CREATORS):
            env.cmd('CONFIG', 'SET', CONFIG, 'legacy')
            reference = f'reference-{i}'
            env.cmd(command, reference, *args)
            legacy = env.cmd('DUMP', reference)
            for policy in ('legacy', 'random-nonmerge', 'random-all'):
                env.cmd('CONFIG', 'SET', CONFIG, policy)
                random = policy == 'random-all' or (
                    policy == 'random-nonmerge' and not command.startswith('CMS.'))
                payloads = []
                for n in range(2):
                    key = f'{policy}-{i}-{n}'
                    env.cmd(command, key, *args)
                    payload = env.cmd('DUMP', key)
                    payloads.append(payload)
                    if random:
                        env.assertNotEqual(payload, legacy)
                    else:
                        env.assertEqual(payload, legacy)
                    # Restore must preserve the stored seed, regardless of policy.
                    env.cmd('RESTORE', key + '-copy', 0, payload)
                    env.assertEqual(env.cmd('DUMP', key + '-copy'), payload)
                if random:
                    env.assertNotEqual(*payloads)

            if i < len(CREATORS):
                # Explicit seeds override every policy (including explicit zero).
                for seed in (0, 123):
                    payloads = []
                    for policy in ('legacy', 'random-nonmerge', 'random-all'):
                        env.cmd('CONFIG', 'SET', CONFIG, policy)
                        key = f'manual-{i}-{seed}-{policy}'
                        env.cmd(command, key, *args, 'SEED', seed)
                        payloads.append(env.cmd('DUMP', key))
                    env.assertEqual(payloads[0], payloads[1])
                    env.assertEqual(payloads[0], payloads[2])
            else:
                # Changing policy must not reseed an existing auto-created filter.
                before = env.cmd('DUMP', reference)
                env.cmd('CONFIG', 'SET', CONFIG, 'legacy')
                env.cmd('RESTORE', reference + '-copy', 0, before)
                env.cmd(command, reference + '-copy', *args)
                env.cmd('CONFIG', 'SET', CONFIG, 'random-all')
                env.cmd(command, reference, *args)
                check = 'BF.EXISTS' if command.startswith('BF.') else 'CF.EXISTS'
                env.assertEqual(1, env.cmd(check, reference, 'item'))
                env.assertEqual(env.cmd('DUMP', reference + '-copy'), env.cmd('DUMP', reference))
    finally:
        env.cmd('CONFIG', 'SET', CONFIG, 'legacy')


def create_policy_keys(env):
    dumps = {}
    for policy in ('legacy', 'random-nonmerge', 'random-all'):
        env.cmd('CONFIG', 'SET', CONFIG, policy)
        for i, (command, args) in enumerate(CREATORS + AUTO_CREATORS):
            key = f'{policy}-{i}'
            env.cmd(command, key, *args)
            dumps[key] = env.cmd('DUMP', key)
    return dumps


def test_seed_policy_replication():
    env = Env(decodeResponses=False, useSlaves=True, freshEnv=True)
    env.skipOnCluster()
    replica = env.getSlaveConnection()
    try:
        # A different replica policy must not change even legacy creations.
        replica.execute_command('CONFIG', 'SET', CONFIG, 'random-all')
        dumps = create_policy_keys(env)
        wait_until(env, lambda: env.cmd('WAIT', 1, 1000) == 1)
        for key, payload in dumps.items():
            env.assertEqual(payload, replica.execute_command('DUMP', key))
    finally:
        env.cmd('CONFIG', 'SET', CONFIG, 'legacy')
        replica.execute_command('CONFIG', 'SET', CONFIG, 'legacy')


def test_seed_policy_aof():
    env = Env(decodeResponses=False, useAof=True, useRdbPreamble=False, freshEnv=True)
    env.skipOnCluster()
    dumps = create_policy_keys(env)
    # Restart resets the runtime policy to legacy. Both regular AOF replay and
    # rewritten AOF must retain concrete random seeds rather than rerandomizing.
    for rewrite in (False, True):
        if rewrite:
            env.dumpAndReload(restart=True)
        else:
            env.stop()
            env.start()
        for key, payload in dumps.items():
            env.assertEqual(payload, env.cmd('DUMP', key))


def test_seed_policy_aof_random_startup():
    # In particular, legacy AOF commands without SEED must remain legacy even
    # when the server starts with random-all configured.
    with tempfile.NamedTemporaryFile(mode='w', suffix='.conf') as config:
        config.write(f'{CONFIG} random-all\n')
        config.flush()
        env = Env(decodeResponses=False, useAof=True, useRdbPreamble=False,
                  freshEnv=True, redisConfigFile=config.name)
        env.skipOnCluster()
        env.assertEqual(env.cmd('CONFIG', 'GET', CONFIG)[1], b'random-all')
        dumps = create_policy_keys(env)
        env.stop()
        env.start()
        env.assertEqual(env.cmd('CONFIG', 'GET', CONFIG)[1], b'random-all')
        for key, payload in dumps.items():
            env.assertEqual(payload, env.cmd('DUMP', key))
        env.stop()


def test_seed_policy_rdb():
    env = Env(decodeResponses=False, freshEnv=True, enableDebugCommand=True)
    env.skipOnCluster()
    try:
        dumps = create_policy_keys(env)
        env.dumpAndReload()
        for key, payload in dumps.items():
            env.assertEqual(payload, env.cmd('DUMP', key))
    finally:
        env.cmd('CONFIG', 'SET', CONFIG, 'legacy')
