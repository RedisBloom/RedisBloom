from common import Env
from rdb_corruption_utils import crc64_redis, encode_len, load_len, rewrite_module_uint
from test_bloom_seed import wait_until


def test_seed_rdb_roundtrip():
    env = Env(decodeResponses=False, freshEnv=True)
    env.skipOnCluster()
    env.skipOnAOF()
    dumps = {}
    for size in (1, 2, 4, 8):
        template = f'template-{size}'
        env.cmd('CMS.INITBYDIM', template, 64, 5, 'CELL_SIZE', size)
        payload = env.cmd('DUMP', template)
        env.assertEqual(2, load_len(payload, 1)[0] & 1023)
        for seed in (0, 123, 0x80000000, 0xffffffff):
            # Only seed empty sketches: changing the seed of populated cells is invalid.
            seeded = rewrite_module_uint(payload, 4, seed)
            for empty in (False, True):
                key = f'{size}-{seed}-{empty}'
                env.cmd('RESTORE', key, 0, seeded)
                env.assertEqual(seeded, env.cmd('DUMP', key))
                if not empty:
                    env.assertEqual([7, 3], env.cmd('CMS.INCRBY', key, b'a\0b', 7, b'', 3))
                dumps[key] = (env.cmd('DUMP', key), empty)
                env.cmd('RESTORE', key + '-copy', 0, dumps[key][0])

    for command in ('SAVE', 'BGSAVE'):
        env.cmd('CONFIG', 'SET', 'save', '')
        env.assertEqual(0, env.cmd('INFO', 'persistence')['aof_enabled'])
        env.cmd(command)
        wait_until(env, lambda: not env.cmd('INFO', 'persistence')['rdb_bgsave_in_progress'])
        env.assertEqual('ok', env.cmd('INFO', 'persistence')['rdb_last_bgsave_status'])
        env.cmd('SET', 'after-snapshot', 'must-not-survive')
        env.stop()
        env.start()
        env.assertEqual(0, env.cmd('EXISTS', 'after-snapshot'))
        for key, (payload, empty) in dumps.items():
            for target in (key, key + '-copy'):
                env.assertEqual(payload, env.cmd('DUMP', target))
                env.assertEqual([0, 0] if empty else [7, 3],
                                env.cmd('CMS.QUERY', target, b'a\0b', b''))
    for key, (_, empty) in dumps.items():
        for target in (key, key + '-copy'):
            env.assertEqual([2 if empty else 9], env.cmd('CMS.INCRBY', target, b'a\0b', 2))
            env.assertEqual([1 if empty else 8], env.cmd('CMS.INCRBY', target, b'a\0b', -1))
        env.assertEqual(env.cmd('DUMP', key), env.cmd('DUMP', key + '-copy'))


def test_seed_legacy_rdb():
    env = Env(decodeResponses=False)
    for size in (1, 2, 4, 8):
        for empty in (False, True):
            key = f'legacy-{size}-{empty}'
            env.cmd('CMS.INITBYDIM', key, 64, 5, 'CELL_SIZE', size)
            if not empty:
                env.cmd('CMS.INCRBY', key, 'a', 7, 'b', 3)
            payload = env.cmd('DUMP', key)
            module_id, _, start, _ = load_len(payload, 1)
            env.assertEqual(b'\x02\x00\x00', payload[-13:-10])  # seed=0, EOF
            # Version 1 has cell size but no trailing seed. Version 0 also omits
            # cell size and always uses 4-byte cells. Generate both in memory.
            for version in ((0, 1) if size == 4 else (1,)):
                fields = payload[start:-13]
                if version == 0:
                    pos = 0
                    for _ in range(3):  # width, depth, counter
                        _, _, pos, _ = load_len(fields, pos)  # UINT opcode
                        _, _, pos, _ = load_len(fields, pos)
                    env.assertEqual(b'\x02\x04', fields[pos:pos + 2])
                    fields = fields[:pos] + fields[pos + 2:]
                body = payload[:1] + encode_len((module_id & ~1023) | version)
                body += fields + b'\x00' + payload[-10:-8]
                target = key + f'-v{version}'
                env.cmd('RESTORE', target, 0, body + crc64_redis(body).to_bytes(8, 'little'))
                env.assertEqual(payload, env.cmd('DUMP', target))
                env.cmd('RESTORE', 'reference', 0, payload, 'REPLACE')
                env.assertEqual(env.cmd('CMS.INCRBY', 'reference', 'a', 2, 'b', 1),
                                env.cmd('CMS.INCRBY', target, 'a', 2, 'b', 1))
                env.assertEqual(env.cmd('DUMP', 'reference'), env.cmd('DUMP', target))


def test_seed_invalid_rdb():
    env = Env(decodeResponses=False)
    env.cmd('CMS.INITBYDIM', 'source', 64, 5)
    payload = env.cmd('DUMP', 'source')
    module_id, _, start, _ = load_len(payload, 1)
    env.assertEqual(b'\x02\x00\x00', payload[-13:-10])
    invalid = [rewrite_module_uint(payload, 4, seed) for seed in (0x100000000, 0xffffffffffffffff)]
    for value in (payload[:-13] + b'\x00',  # Missing seed in version 2.
                  payload[:-13] + b'\x04' + b'\x00' * 9,  # DOUBLE instead of UINT.
                  payload[:1] + encode_len((module_id & ~1023) | 3) + payload[start:-10]):
        body = value + payload[-10:-8]
        invalid.append(body + crc64_redis(body).to_bytes(8, 'little'))
    for bad in invalid:
        with env.assertResponseError():
            env.cmd('RESTORE', 'invalid', 0, bad)
        env.assertEqual(0, env.cmd('EXISTS', 'invalid'))


def test_seed_merge_compatibility():
    env = Env(decodeResponses=False)
    env.cmd('CMS.INITBYDIM', 'template', 64, 5)
    template = env.cmd('DUMP', 'template')
    for seed in (0, 123, 0xffffffff):
        for key in ('dest', 'a', 'b'):
            env.cmd('RESTORE', key, 0, rewrite_module_uint(template, 4, seed), 'REPLACE')
        env.cmd('CMS.INCRBY', 'a', 'item', 3)
        env.cmd('CMS.INCRBY', 'b', 'item', 2)
        env.cmd('CMS.MERGE', 'dest', 2, 'a', 'b', 'WEIGHTS', 2, 3)
        env.assertEqual([12], env.cmd('CMS.QUERY', 'dest', 'item'))
        env.cmd('RESTORE', 'different', 0, rewrite_module_uint(template, 4, seed ^ 1), 'REPLACE')
        for destination, sources in (('dest', ['a', 'different']),
                                     ('different', ['a', 'b']), ('dest', ['dest', 'different'])):
            before = env.cmd('DUMP', destination)
            with env.assertResponseError(contained='seed is not equal'):
                env.cmd('CMS.MERGE', destination, 2, *sources)
            env.assertEqual(before, env.cmd('DUMP', destination))
