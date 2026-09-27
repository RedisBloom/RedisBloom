import struct

from common import Env
from rdb_corruption_utils import crc64_redis, encode_len, load_len, rewrite_module_uint


def test_reserve_seed():
    env = Env(decodeResponses=False)
    env.cmd('CF.RESERVE', 'template', 4)
    template = env.cmd('DUMP', 'template')
    items = [str(i) for i in range(100)]
    keys = []
    for key, options, seed in (
        ('default', [], 0),
        ('zero', ['SEED', 0], 0),
        ('decimal', ['SEED', '123', 'EXPANSION', 2], 123),
        ('hex', ['EXPANSION', 2, 'seed', '0x7b'], 123),
        ('upper', ['SEED', '0x100000000'], 0x100000000),
        ('maximum', ['SEED', '18446744073709551615'], 0xffffffffffffffff),
        ('random', ['SEED', 'RaNdOm'], None),
        ('SEED', [], 0),  # The key name is not an option.
    ):
        env.assertEqual(b'OK', env.cmd('CF.RESERVE', key, 4, *options))
        if seed is None:
            _, header = env.cmd('CF.SCANDUMP', key, 0)
            # A generated zero seed retains the legacy empty response.
            seed = struct.unpack_from('=Q', header, 38)[0] if header else 0
        expected = rewrite_module_uint(template, 8, seed)
        if key in ('decimal', 'hex'):
            expected = rewrite_module_uint(expected, 6, 2)  # Explicit EXPANSION.
        env.assertEqual(expected, env.cmd('DUMP', key))
        env.cmd('CF.INSERT', key, 'ITEMS', *items)
        env.assertEqual([1] * len(items), env.cmd('CF.MEXISTS', key, *items))
        keys.append(key)
    env.assertEqual(env.cmd('DUMP', 'default'), env.cmd('DUMP', 'zero'))
    env.assertEqual(env.cmd('DUMP', 'decimal'), env.cmd('DUMP', 'hex'))
    before = env.cmd('DUMP', 'decimal')
    with env.assertResponseError():
        env.cmd('CF.RESERVE', 'decimal', 4, 'SEED', 456)
    env.assertEqual(before, env.cmd('DUMP', 'decimal'))
    if env.useSlaves:
        env.assertEqual(1, env.cmd('WAIT', 1, 10000))
        replica = env.getSlaveConnection()
        for key in keys:
            env.assertEqual(env.cmd('DUMP', key), replica.execute_command('DUMP', key))


def test_reserve_invalid_seed():
    env = Env()
    for options in (
        ['SEED'], ['SEED', ''], ['SEED', '-1'], ['SEED', '+1'],
        ['SEED', '0x'], ['SEED', 'abc'], ['SEED', '18446744073709551616'],
        ['SEED', '0x10000000000000000'], ['SEED', '1\x00'], ['SEED', ' 1'],
        ['SEED', 1, 'SEED', 2], ['SEED', 1, 'EXPANSION'],
    ):
        with env.assertResponseError():
            env.cmd('CF.RESERVE', 'invalid', 4, *options)
        env.assertEqual(0, env.cmd('EXISTS', 'invalid'))


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
        # Seed an EMPTY filter through RESTORE to exercise the RDB loader directly.
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


def test_seed_scandump_roundtrip():
    env = Env(decodeResponses=False)
    env.cmd('CF.RESERVE', 'template', 4, 'EXPANSION', 2)
    template = env.cmd('DUMP', 'template')
    for seed in (0, 123, 0x100000000, 0xffffffffffffffff):
        for count in (0, 100):
            key = f'cf-{seed}-{count}'
            env.cmd('RESTORE', key, 0, rewrite_module_uint(template, 8, seed))
            items = [str(i) for i in range(count)]
            if items:
                env.cmd('CF.INSERT', key, 'ITEMS', *items)
            if seed == 0 and count == 0:
                # Preserve the existing empty/default-filter SCANDUMP response.
                env.assertEqual([0, None], env.cmd('CF.SCANDUMP', key, 0))
                continue
            chunks = []
            cursor = 0
            while True:
                cursor, chunk = env.cmd('CF.SCANDUMP', key, cursor)
                if cursor == 0:
                    break
                chunks.append((cursor, chunk))
                env.cmd('CF.LOADCHUNK', key + '-copy', cursor, chunk)
            env.assertGreater(len(chunks), 2 if count else 1)
            header = chunks[0][1]
            env.assertEqual(seed, struct.unpack_from('=Q', header, 38)[0])
            env.assertEqual(len(chunks) - 1, struct.unpack_from('=Q', header, 24)[0])
            targets = [key, key + '-copy']
            if seed == 0:
                # Generate the old header in memory: it ends before the seed.
                env.cmd('CF.LOADCHUNK', key + '-legacy', 1, header[:-8])
                for cursor, chunk in chunks[1:]:
                    env.cmd('CF.LOADCHUNK', key + '-legacy', cursor, chunk)
                targets.append(key + '-legacy')
            payload = env.cmd('DUMP', key)
            more = [str(i) for i in range(100, 300)]
            for target in targets:
                env.assertEqual(payload, env.cmd('DUMP', target))
                env.cmd('CF.INSERT', target, 'ITEMS', *more)
                env.assertEqual([1] * len(items + more),
                                env.cmd('CF.MEXISTS', target, *(items + more)))
                env.assertEqual(1, env.cmd('CF.DEL', target, more[0]))
                env.assertEqual([1] * len(items + more[1:]),
                                env.cmd('CF.MEXISTS', target, *(items + more[1:])))
            for target in targets:
                env.assertEqual(env.cmd('DUMP', key), env.cmd('DUMP', target))


def test_seed_invalid_scandump():
    env = Env(decodeResponses=False)
    env.cmd('CF.RESERVE', 'source', 4)
    env.cmd('CF.ADD', 'source', 'item')
    _, header = env.cmd('CF.SCANDUMP', 'source', 0)
    env.assertEqual(46, len(header))
    overflow = bytearray(header)
    struct.pack_into('=Q', overflow, 24, 65537)
    zero_filters = bytearray(header)
    struct.pack_into('=Q', zero_filters, 24, 0)
    for invalid in (header[:24], header[:37], header[:39], header[:-1], header + b'\x00',
                    bytes(overflow), bytes(zero_filters)):
        with env.assertResponseError():
            env.cmd('CF.LOADCHUNK', 'invalid', 1, invalid)
        env.assertEqual(0, env.cmd('EXISTS', 'invalid'))


def test_seed_aof_roundtrip():
    env = Env(decodeResponses=False, useAof=True, useRdbPreamble=False, freshEnv=True)
    env.skipOnCluster()
    env.cmd('CF.RESERVE', 'template', 4, 'EXPANSION', 2)
    template = env.cmd('DUMP', 'template')
    dumps = {}
    for seed in (0, 123, 0x100000000, 0xffffffffffffffff):
        for count in (0, 100):
            key = f'cf-{seed}-{count}'
            env.cmd('RESTORE', key, 0, rewrite_module_uint(template, 8, seed))
            items = [str(i) for i in range(count)]
            if items:
                env.cmd('CF.INSERT', key, 'ITEMS', *items)
            dumps[key] = (env.cmd('DUMP', key), items)
    for rewrite in (False, True):
        if rewrite:
            # With no RDB preamble, this exercises CFAofRewrite / CF.LOADCHUNK.
            env.dumpAndReload(restart=True)
            env.assertEqual('ok', env.cmd('INFO', 'persistence')['aof_last_bgrewrite_status'])
        else:
            env.stop()
            env.start()
        for key, (payload, items) in dumps.items():
            env.assertEqual(payload, env.cmd('DUMP', key))
            if items:
                env.assertEqual([1] * len(items), env.cmd('CF.MEXISTS', key, *items))
            else:
                env.assertEqual(0, env.cmd('CF.EXISTS', key, 'absent'))
            if rewrite:
                more = [str(i) for i in range(100, 300)]
                env.cmd('RESTORE', key + '-reference', 0, payload)
                for target in (key, key + '-reference'):
                    env.cmd('CF.INSERT', target, 'ITEMS', *more)
                    env.assertEqual([1] * len(items + more),
                                    env.cmd('CF.MEXISTS', target, *(items + more)))
                env.assertEqual(env.cmd('DUMP', key), env.cmd('DUMP', key + '-reference'))
