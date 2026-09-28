from common import Env
from rdb_corruption_utils import crc64_redis, encode_len, load_len, rewrite_module_uint
from test_bloom_seed import wait_until


def test_reserve_seed():
    env = Env(decodeResponses=False, useSlaves=True, freshEnv=True)
    env.skipOnCluster()
    dumps = {}
    for params in ([], [64, 5, 0.9]):
        env.cmd('DEL', 'template')
        env.cmd('TOPK.RESERVE', 'template', 3, *params)
        template = env.cmd('DUMP', 'template')
        for name, value, seed in (('default', None, 0), ('zero', '0', 0),
                                  ('decimal', '123', 123), ('hex', '0x7b', 123),
                                  ('upper', '0x80000000', 0x80000000),
                                  ('max', '4294967295', 0xffffffff),
                                  ('zero-fp', '4294965377', 4294965377),
                                  ('random', 'RaNdOm', None)):
            key = f'{len(params)}-{name}'
            options = [] if value is None else ['sEeD', value]
            env.assertEqual(b'OK', env.cmd('TOPK.RESERVE', key, 3, *params, *options))
            payload = env.cmd('DUMP', key)
            if seed is not None:
                env.assertEqual(rewrite_module_uint(template, 3, seed), payload)
            dumps[key] = payload
            with env.assertResponseError(contained='key already exists'):
                env.cmd('TOPK.RESERVE', key, 3, 'SEED', 'random')
            env.assertEqual(payload, env.cmd('DUMP', key))
    wait_until(env, lambda: env.cmd('WAIT', 1, 1000) == 1)
    replica = env.getSlaveConnection()
    for key, payload in dumps.items():
        # Empty heaps have no pointers, so their serialized bytes are comparable.
        env.assertEqual(payload, replica.execute_command('DUMP', key))
        env.cmd('TOPK.INCRBY', key, '', 7)
        env.assertEqual([7], env.cmd('TOPK.COUNT', key, ''))
        env.assertEqual([1], env.cmd('TOPK.QUERY', key, ''))
    wait_until(env, lambda: env.cmd('WAIT', 1, 1000) == 1)
    for key in dumps:
        env.assertEqual([7], replica.execute_command('TOPK.COUNT', key, ''))
        env.assertEqual([1], replica.execute_command('TOPK.QUERY', key, ''))


def test_reserve_invalid_seed():
    env = Env(decodeResponses=False)
    for params in ([], [64, 5, 0.9]):
        for options in (['SEED'], ['SEED', ''], ['SEED', '-1'], ['SEED', '+1'],
                        ['SEED', '0x'], ['SEED', 'abc'], ['SEED', '4294967296'],
                        ['SEED', '0x100000000'], ['SEED', '18446744073709551615'],
                        ['SEED', '1\x00'], ['SEED', ' 1'], ['SEED\x00', 1],
                        ['SEED', 1, 'SEED', 2], ['SEED', 'SEED'], ['UNKNOWN', 1],
                        ['SEED', 1, 'UNKNOWN', 2]):
            with env.assertResponseError():
                env.cmd('TOPK.RESERVE', 'invalid', 3, *params, *options)
            env.assertEqual(0, env.cmd('EXISTS', 'invalid'))
    for args in ([3, 'SEED', 1, 64, 5, 0.9], [3, 64, 'SEED', 1],
                 [3, 0, 5, 0.9, 'SEED', 1], [3, 64, 0, 0.9, 'SEED', 1],
                 [3, 64, 5, 0, 'SEED', 1], [0, 'SEED', 1]):
        with env.assertResponseError():
            env.cmd('TOPK.RESERVE', 'invalid', *args)
        env.assertEqual(0, env.cmd('EXISTS', 'invalid'))
    env.assertEqual(b'OK', env.cmd('TOPK.RESERVE', 'SEED', 3, 'SEED', 1))


def test_reserve_seed_aof_replay():
    env = Env(decodeResponses=False, useAof=True, useRdbPreamble=False, freshEnv=True)
    env.skipOnCluster()
    dumps = {}
    for params in ([], [64, 5, 0.9]):
        for value in (None, 'random', '0xffffffff'):
            key = f'{len(params)}-{value}'
            options = [] if value is None else ['SEED', value]
            env.cmd('TOPK.RESERVE', key, 3, *params, *options)
            dumps[key] = env.cmd('DUMP', key)
    env.stop()
    env.start()
    for key, payload in dumps.items():
        env.assertEqual(payload, env.cmd('DUMP', key))
        env.cmd('TOPK.INCRBY', key, 'item', 7)
    env.dumpAndReload(restart=True)
    for key in dumps:
        env.assertEqual([7], env.cmd('TOPK.COUNT', key, 'item'))
        env.assertEqual([1], env.cmd('TOPK.QUERY', key, 'item'))


def check_seed_reload(env):
    env.cmd('TOPK.RESERVE', 'template', 3, 64, 5, 0.9)
    template = env.cmd('DUMP', 'template')
    env.assertEqual(1, load_len(template, 1)[0] & 1023)
    cases = []
    for seed in (0, 123, 0x80000000, 0xffffffff, 0xffffffff - 1918):
        for empty in (False, True):
            key = f'{seed}-{empty}'
            env.cmd('TOPK.RESERVE', key, 3, 64, 5, 0.9, 'SEED', seed)
            env.assertEqual(rewrite_module_uint(template, 3, seed), env.cmd('DUMP', key))
            if not empty:
                env.cmd('TOPK.INCRBY', key, '', 7)
            env.cmd('RESTORE', key + '-copy', 0, env.cmd('DUMP', key))
            cases.extend([(key, seed, empty), (key + '-copy', seed, empty)])
    for reloaded in (False, True):
        if reloaded:
            env.dumpAndReload(restart=True)
        for key, seed, empty in cases:
            # TopK serializes heap pointers, so full DUMPs need not be byte-identical.
            suffix = b'\x02' + encode_len(seed) + b'\x00'
            env.assertTrue(env.cmd('DUMP', key)[:-10].endswith(suffix))
            env.assertEqual([0 if empty else 7], env.cmd('TOPK.COUNT', key, ''))
            env.assertEqual([0 if empty else 1], env.cmd('TOPK.QUERY', key, ''))
            env.assertEqual([] if empty else [b'', 7], env.cmd('TOPK.LIST', key, 'WITHCOUNT'))
    for key, _, empty in cases:
        env.cmd('TOPK.INCRBY', key, '', 3)
        env.assertEqual([3 if empty else 10], env.cmd('TOPK.COUNT', key, ''))
        env.assertEqual([1], env.cmd('TOPK.QUERY', key, ''))


def test_seed_rdb_roundtrip():
    env = Env(decodeResponses=False, freshEnv=True)
    env.skipOnCluster()
    env.skipOnAOF()
    check_seed_reload(env)


def test_seed_aof_rewrite():
    env = Env(decodeResponses=False, useAof=True, useRdbPreamble=False, freshEnv=True)
    env.skipOnCluster()
    check_seed_reload(env)
    env.assertEqual('ok', env.cmd('INFO', 'persistence')['aof_last_bgrewrite_status'])


def test_seed_legacy_rdb():
    env = Env(decodeResponses=False)
    for empty in (False, True):
        key = f'legacy-{empty}'
        env.cmd('TOPK.RESERVE', key, 3, 64, 5, 0.9)
        if not empty:
            env.cmd('TOPK.INCRBY', key, 'item', 7)
        payload = env.cmd('DUMP', key)
        module_id, _, start, _ = load_len(payload, 1)
        env.assertEqual(b'\x02\x00\x00', payload[-13:-10])
        # Generate a version-0 payload in memory: remove the trailing seed.
        body = payload[:1] + encode_len(module_id & ~1023) + payload[start:-13]
        body += b'\x00' + payload[-10:-8]
        target = key + '-copy'
        env.cmd('RESTORE', target, 0, body + crc64_redis(body).to_bytes(8, 'little'))
        env.assertEqual(b'\x02\x00\x00', env.cmd('DUMP', target)[-13:-10])
        for command in ('TOPK.COUNT', 'TOPK.QUERY'):
            env.assertEqual(env.cmd(command, key, 'item'), env.cmd(command, target, 'item'))
        for name in (key, target):
            env.cmd('TOPK.INCRBY', name, 'item', 3)
            env.assertEqual([3 if empty else 10], env.cmd('TOPK.COUNT', name, 'item'))


def test_seed_invalid_rdb():
    env = Env(decodeResponses=False)
    env.cmd('TOPK.RESERVE', 'source', 3, 64, 5, 0.9)
    for populated in (False, True):
        if populated:
            env.cmd('TOPK.ADD', 'source', 'item')
        payload = env.cmd('DUMP', 'source')
        module_id, _, start, _ = load_len(payload, 1)
        invalid = [rewrite_module_uint(payload, 3, seed)
                   for seed in (0x100000000, 0xffffffffffffffff)]
        for value in (payload[:-13] + b'\x00',  # Missing seed in version 1.
                      payload[:-13] + b'\x04' + b'\x00' * 9,  # DOUBLE instead of UINT.
                      payload[:1] + encode_len((module_id & ~1023) | 2) + payload[start:-10]):
            body = value + payload[-10:-8]
            invalid.append(body + crc64_redis(body).to_bytes(8, 'little'))
        for bad in invalid:
            with env.assertResponseError():
                env.cmd('RESTORE', 'invalid', 0, bad)
            env.assertEqual(0, env.cmd('EXISTS', 'invalid'))
