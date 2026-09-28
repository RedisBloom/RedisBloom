from common import Env
from rdb_corruption_utils import crc64_redis, encode_len, load_len, rewrite_module_uint


def check_seed_reload(env):
    env.cmd('TOPK.RESERVE', 'template', 3, 64, 5, 0.9)
    template = env.cmd('DUMP', 'template')
    env.assertEqual(1, load_len(template, 1)[0] & 1023)
    cases = []
    for seed in (0, 123, 0x80000000, 0xffffffff, 0xffffffff - 1918):
        # Until RESERVE accepts SEED, seed only EMPTY sketches through RESTORE.
        payload = rewrite_module_uint(template, 3, seed)
        for empty in (False, True):
            key = f'{seed}-{empty}'
            env.cmd('RESTORE', key, 0, payload)
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
