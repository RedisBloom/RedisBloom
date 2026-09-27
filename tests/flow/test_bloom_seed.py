import struct

from common import Env
from rdb_corruption_utils import crc64_redis, encode_len, load_len


def test_reserve_seed():
    env = Env(decodeResponses=False)
    items = [str(i) for i in range(100)]

    # Equal decimal and hex seeds must produce identical filter contents.
    for key, options in [
        ('decimal', ['SEED', '123', 'EXPANSION', 2]),
        ('hex', ['EXPANSION', 2, 'SEED', '0x7b']),
        ('zero', ['SEED', '0']),
        ('maximum', ['SEED', '18446744073709551615']),
        ('random', ['SEED', 'RaNdOm']),
        ('legacy', []),
        ('explicit-default', ['SEED', '0xc6a4a7935bd1e995']),
    ]:
        env.assertEqual(b'OK', env.cmd('BF.RESERVE', key, 0.000001, 4, *options))
        env.cmd('BF.MADD', key, *items)
        env.assertEqual([1] * len(items), env.cmd('BF.MEXISTS', key, *items))
        env.assertEqual([0] * len(items), env.cmd('BF.MADD', key, *items))

    def chunks(key):
        result = []
        cursor = 0
        while True:
            cursor, data = env.cmd('BF.SCANDUMP', key, cursor)
            if cursor == 0:
                return result
            result.append(data)

    env.assertEqual(chunks('decimal'), chunks('hex'))
    env.assertEqual(chunks('legacy'), chunks('explicit-default'))
    env.assertTrue(chunks('zero') != chunks('maximum'))

    before = chunks('decimal')
    with env.assertResponseError():
        env.cmd('BF.RESERVE', 'decimal', 0.001, 100, 'SEED', 456)
    env.assertEqual(before, chunks('decimal'))
    env.assertEqual(b'OK', env.cmd('BF.RESERVE', 'nonscaling', 0.001, 100, 'SEED', 1, 'NONSCALING'))

    if env.useSlaves:
        env.assertEqual(1, env.cmd('WAIT', 1, 10000))
        replica = env.getSlaveConnection()
        for key in ('decimal', 'hex', 'zero', 'maximum', 'random', 'legacy'):
            env.assertEqual([1] * len(items), replica.execute_command('BF.MEXISTS', key, *items))


def test_reserve_invalid_seed():
    env = Env()
    for options in [
        ['SEED'], ['SEED', ''], ['SEED', '-1'], ['SEED', '0x'],
        ['SEED', 'abc'], ['SEED', '18446744073709551616'],
        ['SEED', '0x10000000000000000'], ['SEED', '1\x00'],
        ['SEED', 1, 'SEED', 2], ['SEED', 1, 'EXPANSION'],
    ]:
        with env.assertResponseError():
            env.cmd('BF.RESERVE', 'invalid', 0.001, 100, *options)
        env.assertEqual(0, env.cmd('EXISTS', 'invalid'))


def test_reserve_legacy_arguments():
    env = Env(decodeResponses=True)
    # Seed support must not tighten legacy handling of unrelated arguments.
    env.assertOk(env.cmd('BF.RESERVE', 'legacy-args', 0.001, 100, 'unused'))
    with env.assertResponseError():
        env.cmd('BF.RESERVE', 'too-many-args', 0.001, 100, 'a', 'b', 'c', 'd')
    env.assertEqual(0, env.cmd('EXISTS', 'too-many-args'))


def test_seed_rdb_roundtrip():
    env = Env(decodeResponses=False)
    items = [str(i) for i in range(100)]
    dumps = {}
    for key, options in [('default', []), ('zero', ['SEED', 0]),
                         ('maximum', ['SEED', '0xffffffffffffffff']),
                         ('random', ['SEED', 'random'])]:
        env.cmd('BF.RESERVE', key, 0.000001, 4, *options)
        env.cmd('BF.MADD', key, *items)
        dumps[key] = env.cmd('DUMP', key)
        env.cmd('RESTORE', key + '-copy', 0, dumps[key])
        env.assertEqual(dumps[key], env.cmd('DUMP', key + '-copy'))
        env.assertEqual([1] * len(items), env.cmd('BF.MEXISTS', key + '-copy', *items))

    env.dumpAndReload(restart=True)
    for key, payload in dumps.items():
        env.assertEqual(payload, env.cmd('DUMP', key))
        env.assertEqual([1] * len(items), env.cmd('BF.MEXISTS', key, *items))
        # New writes and expansion must continue using the restored seed.
        more = [str(i) for i in range(100, 300)]
        env.cmd('BF.MADD', key, *more)
        env.assertEqual([1] * 300, env.cmd('BF.MEXISTS', key, *(items + more)))


def test_seed_legacy_rdb():
    env = Env(decodeResponses=False)
    env.cmd('BF.RESERVE', 'source', 0.001, 100)
    env.cmd('BF.ADD', 'source', 'apple')
    payload = env.cmd('DUMP', 'source')
    module_id, _, start, _ = load_len(payload, 1)
    env.assertEqual(5, module_id & 1023)
    seed_field = bytes([2]) + encode_len(0xc6a4a7935bd1e995)
    env.assertEqual(seed_field + bytes([0]), payload[-10-len(seed_field)-1:-10])

    # Version 4 has exactly the same fields, without the trailing seed.
    value = payload[:1] + encode_len((module_id & ~1023) | 4)
    value += payload[start:-10-len(seed_field)-1] + bytes([0])
    body = value + payload[-10:-8]
    legacy = body + crc64_redis(body).to_bytes(8, 'little')
    env.cmd('RESTORE', 'legacy-restored', 0, legacy)
    env.assertEqual(1, env.cmd('BF.EXISTS', 'legacy-restored', 'apple'))
    env.assertEqual(payload, env.cmd('DUMP', 'legacy-restored'))

    # A version 5 payload missing its seed must fail instead of defaulting.
    body = payload[:start] + payload[start:-10-len(seed_field)-1] + bytes([0]) + payload[-10:-8]
    truncated = body + crc64_redis(body).to_bytes(8, 'little')
    with env.assertResponseError():
        env.cmd('RESTORE', 'missing-seed', 0, truncated)
    env.assertEqual(0, env.cmd('EXISTS', 'missing-seed'))


def test_seed_scandump_roundtrip():
    env = Env(decodeResponses=False)
    items = [str(i) for i in range(100)]
    for key, options in [('default', []), ('zero', ['SEED', 0]),
                         ('maximum', ['SEED', '0xffffffffffffffff']),
                         ('random', ['SEED', 'random'])]:
        env.cmd('BF.RESERVE', key, 0.000001, 4, *options)
        env.cmd('BF.MADD', key, *items)
        cursor = 0
        chunks = []
        while True:
            cursor, data = env.cmd('BF.SCANDUMP', key, cursor)
            if cursor == 0:
                break
            chunks.append((cursor, data))
            env.cmd('BF.LOADCHUNK', key + '-copy', cursor, data)
        env.assertTrue(len(chunks) > 2)  # Includes multiple expanded filters.
        env.assertEqual(env.cmd('DUMP', key), env.cmd('DUMP', key + '-copy'))
        env.assertEqual([1] * len(items), env.cmd('BF.MEXISTS', key + '-copy', *items))

        more = [str(i) for i in range(100, 300)]
        env.cmd('BF.MADD', key, *more)
        env.cmd('BF.MADD', key + '-copy', *more)
        env.assertEqual(env.cmd('DUMP', key), env.cmd('DUMP', key + '-copy'))

        if key == 'default':
            # Old dumps omit the trailing seed and its dump-only flag.
            header = bytearray(chunks[0][1][:-8])
            flags = struct.unpack_from('=I', header, 12)[0]
            struct.pack_into('=I', header, 12, flags & ~(1 << 31))
            env.cmd('BF.LOADCHUNK', 'old-dump', 1, bytes(header))
            for pos, data in chunks[1:]:
                env.cmd('BF.LOADCHUNK', 'old-dump', pos, data)
            env.assertEqual([1] * len(items), env.cmd('BF.MEXISTS', 'old-dump', *items))

        if env.useSlaves:
            env.assertEqual(1, env.cmd('WAIT', 1, 10000))
            env.assertEqual([1] * 300, env.getSlaveConnection().execute_command(
                'BF.MEXISTS', key + '-copy', *(items + more)))


def test_seed_scandump_invalid_header():
    env = Env(decodeResponses=False)
    env.cmd('BF.RESERVE', 'source', 0.001, 100)
    _, header = env.cmd('BF.SCANDUMP', 'source', 0)
    narrow = bytearray(header)
    flags = struct.unpack_from('=I', narrow, 12)[0]
    struct.pack_into('=I', narrow, 12, flags & ~4)  # 32-bit filter, 64-bit seed.
    for invalid in (header[:-1], header[:-8], header + b'\0', bytes(narrow)):
        with env.assertResponseError():
            env.cmd('BF.LOADCHUNK', 'invalid-dump', 1, invalid)
        env.assertEqual(0, env.cmd('EXISTS', 'invalid-dump'))


def test_seed_aof_roundtrip():
    # Disable the RDB preamble so rewrite exercises BFAofRewrite / BF.LOADCHUNK.
    env = Env(decodeResponses=False, useAof=True, useRdbPreamble=False, freshEnv=True)
    env.skipOnCluster()
    items = [str(i) for i in range(100)]
    dumps = {}
    for key, options in [('default', []), ('zero', ['SEED', 0]),
                         ('manual', ['SEED', 123]),
                         ('maximum', ['SEED', '0xffffffffffffffff']),
                         ('random', ['SEED', 'random'])]:
        env.cmd('BF.RESERVE', key, 0.000001, 4, *options)
        env.cmd('BF.MADD', key, *items)
        dumps[key] = env.cmd('DUMP', key)

    for rewrite in (False, True):
        if rewrite:
            env.dumpAndReload(restart=True)
            env.assertEqual('ok', env.cmd('INFO', 'persistence')['aof_last_bgrewrite_status'])
        else:
            # Restart without rewriting: replay BF.RESERVE with its concrete seed.
            env.stop()
            env.start()

        more = [str(i) for i in range(len(items), len(items) + 200)]
        for key, payload in dumps.items():
            # Byte equality checks the seed as well as the filter contents.
            env.assertEqual(payload, env.cmd('DUMP', key))
            env.assertEqual([1] * len(items), env.cmd('BF.MEXISTS', key, *items))
            env.cmd('BF.MADD', key, *more)
            env.assertEqual([1] * len(more), env.cmd('BF.MEXISTS', key, *more))
            dumps[key] = env.cmd('DUMP', key)
        items += more
