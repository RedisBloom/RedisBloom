from common import Env
from rdb_corruption_utils import crc64_redis, encode_len, load_len, rewrite_module_uint


def test_seed_rdb_roundtrip():
    env = Env(decodeResponses=False, freshEnv=True)
    env.skipOnCluster()
    env.skipOnAOF()
    env.cmd('CONFIG', 'SET', 'save', '')
    env.cmd('CF.RESERVE', 'template', 4, 'EXPANSION', 2)
    template = env.cmd('DUMP', 'template')
    module_id, _, _, _ = load_len(template, 1)
    env.assertEqual(5, module_id & 1023)
    env.assertEqual(b'\x02\x00\x00', template[-13:-10])  # UINT seed=0, EOF.
    dumps = {}
    for seed in (0, 123, 0x100000000, 0xffffffffffffffff):
        # Until CF.RESERVE SEED is exposed, seed an EMPTY filter through RESTORE.
        # Seven header integers + one subfilter bucket count precede the seed.
        seeded = rewrite_module_uint(template, 8, seed)
        for count in (0, 100):
            key = f'cf-{seed}-{count}'
            env.cmd('RESTORE', key, 0, seeded)
            env.assertEqual(seeded, env.cmd('DUMP', key))
            items = [str(i) for i in range(count)]
            if items:
                env.assertEqual([1] * count, env.cmd('CF.INSERT', key, 'ITEMS', *items))
                info = env.cmd('CF.INFO', key)
                env.assertGreater(dict(zip(info[::2], info[1::2]))[b'Number of filters'], 1)
            payload = env.cmd('DUMP', key)
            dumps[key] = (payload, items)
            env.cmd('RESTORE', key + '-copy', 0, payload)
            env.assertEqual(payload, env.cmd('DUMP', key + '-copy'))

    # Explicit RDB-only restart; shutdown must not replace the saved snapshot.
    env.assertEqual(0, env.cmd('INFO', 'persistence')['aof_enabled'])
    env.cmd('SAVE')
    env.cmd('SET', 'after-save', 'must-not-survive')
    env.stop()
    env.start()
    env.assertEqual(0, env.cmd('EXISTS', 'after-save'))
    env.assertEqual(0, env.cmd('INFO', 'persistence')['aof_enabled'])
    more = [str(i) for i in range(100, 300)]
    for key, (payload, items) in dumps.items():
        for target in (key, key + '-copy'):
            env.assertEqual(payload, env.cmd('DUMP', target))
            if items:
                env.assertEqual([1] * len(items), env.cmd('CF.MEXISTS', target, *items))
            else:
                env.assertEqual([0] * len(more), env.cmd('CF.MEXISTS', target, *more))
            env.assertEqual([1] * len(more), env.cmd('CF.INSERT', target, 'ITEMS', *more))
            env.assertEqual([1] * len(items + more),
                            env.cmd('CF.MEXISTS', target, *(items + more)))
            env.assertGreaterEqual(env.cmd('CF.COUNT', target, more[0]), 1)
            env.assertEqual(1, env.cmd('CF.DEL', target, more[0]))
            env.assertEqual([1] * len(items + more[1:]),
                            env.cmd('CF.MEXISTS', target, *(items + more[1:])))
        env.assertEqual(env.cmd('DUMP', key), env.cmd('DUMP', key + '-copy'))


def test_seed_legacy_rdb():
    env = Env(decodeResponses=False)
    for count in (0, 1, 100):
        key = f'legacy-{count}'
        env.cmd('CF.RESERVE', key, 4, 'EXPANSION', 2)
        items = [str(i) for i in range(count)]
        if items:
            env.cmd('CF.INSERT', key, 'ITEMS', *items)
        payload = env.cmd('DUMP', key)
        module_id, _, start, _ = load_len(payload, 1)
        env.assertEqual(5, module_id & 1023)
        env.assertEqual(b'\x02\x00\x00', payload[-13:-10])
        # Version 4 has the same layout without the final UINT seed field.
        body = payload[:1] + encode_len((module_id & ~1023) | 4)
        body += payload[start:-13] + b'\x00' + payload[-10:-8]
        env.cmd('RESTORE', key + '-copy', 0, body + crc64_redis(body).to_bytes(8, 'little'))
        env.assertEqual(payload, env.cmd('DUMP', key + '-copy'))
        more = [str(i) for i in range(100, 300)]
        for target in (key, key + '-copy'):
            env.cmd('CF.INSERT', target, 'ITEMS', *more)
            env.assertEqual([1] * len(items + more),
                            env.cmd('CF.MEXISTS', target, *(items + more)))
        env.assertEqual(env.cmd('DUMP', key), env.cmd('DUMP', key + '-copy'))


def test_seed_invalid_rdb():
    env = Env(decodeResponses=False)
    env.cmd('CF.RESERVE', 'source', 4)
    payload = env.cmd('DUMP', 'source')
    module_id, _, start, _ = load_len(payload, 1)
    env.assertEqual(5, module_id & 1023)
    env.assertEqual(b'\x02\x00\x00', payload[-13:-10])
    for value in (
        payload[:-13] + b'\x00',  # Missing seed, but version is still 5.
        payload[:-13] + b'\x04' + b'\x00' * 9,  # DOUBLE instead of UINT, then EOF.
        payload[:1] + encode_len((module_id & ~1023) | 6) + payload[start:-10],
    ):
        body = value + payload[-10:-8]
        with env.assertResponseError():
            env.cmd('RESTORE', 'invalid', 0, body + crc64_redis(body).to_bytes(8, 'little'))
        env.assertEqual(0, env.cmd('EXISTS', 'invalid'))
