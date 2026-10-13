from common import Env
from rdb_corruption_utils import crc64_redis, encode_len, load_len, rewrite_module_uint
from test_bloom_seed import wait_until


INIT_COMMANDS = [('CMS.INITBYDIM', [64, 5]), ('CMS.INITBYPROB', [0.03125, 0.03125])]
SEED_CASES = [('default', []), ('zero', ['SEED', 0]), ('manual', ['SEED', 123]),
              ('upper', ['SEED', '0x80000000']), ('maximum', ['SEED', '0xffffffff']),
              ('random', ['SEED', 'random'])]


def test_init_seed():
    env = Env(decodeResponses=False)
    keys = []
    for size in (1, 2, 4, 8):
        template = f'template-{size}'
        env.cmd('CMS.INITBYDIM', template, 64, 5, 'CELL_SIZE', size)
        payload = env.cmd('DUMP', template)
        for command, dimensions in INIT_COMMANDS:
            for name, value, seed in (('default', None, 0), ('zero', '0', 0),
                                      ('decimal', '123', 123), ('hex', '0x7b', 123),
                                      ('upper', '0x80000000', 0x80000000),
                                      ('maximum', '4294967295', 0xffffffff),
                                      ('random', 'RaNdOm', None)):
                key = f'{command}-{size}-{name}'
                options = [] if value is None else ['sEeD', value]
                # Exercise both option orders and the omitted CELL_SIZE path.
                if size != 4:
                    options = (['CELL_SIZE', size] + options if size == 1 else
                               options + ['CELL_SIZE', size])
                env.assertEqual(b'OK', env.cmd(command, key, *dimensions, *options))
                if seed is not None:
                    env.assertEqual(rewrite_module_uint(payload, 4, seed), env.cmd('DUMP', key))
                before = env.cmd('DUMP', key)
                env.cmd('RESTORE', key + '-copy', 0, before)
                with env.assertResponseError(contained='key already exists'):
                    env.cmd(command, key, *dimensions, 'SEED', 'random')
                env.assertEqual(before, env.cmd('DUMP', key))
                for target in (key, key + '-copy'):
                    env.assertEqual([7, 3], env.cmd('CMS.INCRBY', target, b'a\0b', 7, b'', 3))
                    env.assertEqual([7, 3], env.cmd('CMS.QUERY', target, b'a\0b', b''))
                env.assertEqual(env.cmd('DUMP', key), env.cmd('DUMP', key + '-copy'))
                keys.append(key)
    if env.useSlaves:
        wait_until(env, lambda: env.cmd('WAIT', 1, 1000) == 1)
        replica = env.getSlaveConnection()
        for key in keys:
            env.assertEqual(env.cmd('DUMP', key), replica.execute_command('DUMP', key))


def test_init_invalid_seed():
    env = Env(decodeResponses=False)
    for command, dimensions in INIT_COMMANDS:
        for options in (['SEED'], ['SEED', ''], ['SEED', '-1'], ['SEED', '+1'],
                        ['SEED', '0x'], ['SEED', 'abc'], ['SEED', '4294967296'],
                        ['SEED', '0x100000000'], ['SEED', '18446744073709551615'],
                        ['SEED', '1\x00'], ['SEED', ' 1'], ['SEED\x00', 1],
                        ['SEED', 1, 'SEED', 2], ['CELL_SIZE', 1, 'CELL_SIZE', 2],
                        ['SEED', 1, 'CELL_SIZE'], ['SEED', 1, 'CELL_SIZE', 3],
                        ['SEED', 'CELL_SIZE'], ['CELL_SIZE', 'SEED'],
                        ['CELL_SIZE', 1, 'UNKNOWN', 2], ['CELL_SIZE\x00', 1],
                        ['UNKNOWN', 1, 'SEED', 2], ['SEED', 1, 'UNKNOWN', 2],
                        ['UNKNOWN', 'CELL_SIZE', 1, 2], ['UNKNOWN', 'SEED', 1, 2]):
            with env.assertResponseError():
                env.cmd(command, 'invalid', *dimensions, *options)
            env.assertEqual(0, env.cmd('EXISTS', 'invalid'))
        # Required argument/key values must not be mistaken for option tokens.
        env.cmd('DEL', 'SEED')
        env.assertEqual(b'OK', env.cmd(command, 'SEED', *dimensions))


def test_init_seed_aof():
    env = Env(decodeResponses=False, useAof=True, useRdbPreamble=False, freshEnv=True)
    env.skipOnCluster()
    dumps = {}
    for command, dimensions in INIT_COMMANDS:
        for size in (1, 2, 4, 8):
            for name, options in SEED_CASES:
                for empty in (False, True):
                    key = f'{command}-{size}-{name}-{empty}'
                    env.cmd(command, key, *dimensions, 'CELL_SIZE', size, *options)
                    if not empty:
                        env.cmd('CMS.INCRBY', key, 'item', 7)
                    dumps[key] = (env.cmd('DUMP', key), empty)
    for rewrite in (False, True):
        if rewrite:
            env.dumpAndReload(restart=True)
            env.assertEqual('ok', env.cmd('INFO', 'persistence')['aof_last_bgrewrite_status'])
        else:
            env.stop()
            env.start()
        for key, (payload, empty) in dumps.items():
            env.assertEqual(payload, env.cmd('DUMP', key))
            env.assertEqual([0 if empty else 7], env.cmd('CMS.QUERY', key, 'item'))
            if rewrite:
                env.cmd('RESTORE', 'reference', 0, payload, 'REPLACE')
                for target in (key, 'reference'):
                    env.cmd('CMS.INCRBY', target, 'item', 2, b'\xff\0', 3)
                    env.cmd('CMS.INCRBY', target, 'item', -1)
                env.assertEqual(env.cmd('DUMP', 'reference'), env.cmd('DUMP', key))


def test_seed_rdb_roundtrip():
    env = Env(decodeResponses=False, freshEnv=True)
    env.skipOnCluster()
    env.skipOnAOF()
    dumps = {}
    for command, dimensions in INIT_COMMANDS:
        for size in (1, 2, 4, 8):
            for name, options in SEED_CASES:
                for empty in (False, True):
                    key = f'{command}-{size}-{name}-{empty}'
                    env.cmd(command, key, *dimensions, 'CELL_SIZE', size, *options)
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
    # Shared hash tag keeps all merge keys in the same Redis Cluster slot.
    dest, a, b, different = ('{cms}:dest', '{cms}:a', '{cms}:b', '{cms}:different')
    env.cmd('CMS.INITBYDIM', 'template', 64, 5)
    template = env.cmd('DUMP', 'template')
    for seed in (0, 123, 0xffffffff):
        for key in (dest, a, b):
            env.cmd('RESTORE', key, 0, rewrite_module_uint(template, 4, seed), 'REPLACE')
        env.cmd('CMS.INCRBY', a, 'item', 3)
        env.cmd('CMS.INCRBY', b, 'item', 2)
        env.cmd('CMS.MERGE', dest, 2, a, b, 'WEIGHTS', 2, 3)
        env.assertEqual([12], env.cmd('CMS.QUERY', dest, 'item'))
        env.cmd('RESTORE', different, 0, rewrite_module_uint(template, 4, seed ^ 1), 'REPLACE')
        for destination, sources in ((dest, [a, different]),
                                     (different, [a, b]), (dest, [dest, different])):
            before = env.cmd('DUMP', destination)
            with env.assertResponseError(contained='seed is not equal'):
                env.cmd('CMS.MERGE', destination, 2, *sources)
            env.assertEqual(before, env.cmd('DUMP', destination))


def test_seed_public_merge():
    env = Env(decodeResponses=False)
    # Shared hash tag keeps MERGE and multi-key DEL valid in Redis Cluster.
    destination, a, b, different_key = ('{cms}:dest', '{cms}:a', '{cms}:b', '{cms}:different')
    for command, dimensions in INIT_COMMANDS:
        for size in (1, 2, 4, 8):
            for _, options in SEED_CASES:
                env.cmd('DEL', a, b, destination, different_key)
                env.cmd(command, a, *dimensions, 'CELL_SIZE', size, *options)
                empty = env.cmd('DUMP', a)
                # Copy the empty sketch so random-seeded sources share the same seed.
                for key in (b, destination):
                    env.cmd('RESTORE', key, 0, empty)
                env.cmd('CMS.INCRBY', a, 'item', 3)
                env.cmd('CMS.INCRBY', b, 'item', 2)
                env.cmd('CMS.MERGE', destination, 2, a, b, 'WEIGHTS', 2, 3)
                env.assertEqual([12], env.cmd('CMS.QUERY', destination, 'item'))
                env.cmd('CMS.MERGE', destination, 2, destination, a, 'WEIGHTS', 1, -1)
                env.assertEqual([9], env.cmd('CMS.QUERY', destination, 'item'))
                before = env.cmd('DUMP', destination)
                with env.assertResponseError():
                    env.cmd('CMS.MERGE', destination, 1, a, 'WEIGHTS', -1)
                env.assertEqual(before, env.cmd('DUMP', destination))
                different = rewrite_module_uint(empty, 4, 0)
                if different == empty:
                    different = rewrite_module_uint(empty, 4, 1)
                env.cmd('RESTORE', different_key, 0, different)
                for dest, sources in ((destination, [a, different_key]), (different_key, [a, b])):
                    before = env.cmd('DUMP', dest)
                    with env.assertResponseError(contained='seed is not equal'):
                        env.cmd('CMS.MERGE', dest, 2, *sources)
                    env.assertEqual(before, env.cmd('DUMP', dest))


def test_seed_replica_recovery():
    env = Env(useSlaves=True, decodeResponses=False, freshEnv=True)
    env.skipOnCluster()
    master = env.getConnection()
    replica = env.getSlaveConnection()
    keys = []
    for command, dimensions in INIT_COMMANDS:
        for size in (1, 2, 4, 8):
            for name, options in SEED_CASES:
                for empty in (False, True):
                    key = f'{command}-{size}-{name}-{empty}'
                    master.execute_command(command, key, *dimensions, 'CELL_SIZE', size, *options)
                    if not empty:
                        master.execute_command('CMS.INCRBY', key, 'item', 7)
                    keys.append(key)
    wait_until(env, lambda: master.execute_command('WAIT', 1, 1000) == 1)
    for key in keys:
        env.assertEqual(master.execute_command('DUMP', key), replica.execute_command('DUMP', key))

    master.execute_command('CLIENT', 'KILL', 'TYPE', 'replica')
    changed = keys[-2]  # Populated random-seeded sketch.
    master.execute_command('CMS.INCRBY', changed, 'item', 1)
    wait_until(env, lambda: master.execute_command('WAIT', 1, 1000) == 1)
    env.assertEqual(master.execute_command('DUMP', changed), replica.execute_command('DUMP', changed))

    replication = replica.info('replication')
    full_syncs = master.info('stats')['sync_full']
    replica.execute_command('REPLICAOF', 'NO', 'ONE')
    replica.execute_command('FLUSHALL')
    replica.execute_command('REPLICAOF', replication['master_host'], replication['master_port'])
    wait_until(env, lambda: master.info('stats')['sync_full'] > full_syncs)
    wait_until(env, lambda: master.execute_command('WAIT', 1, 1000) == 1)
    for key in keys:
        env.assertEqual(master.execute_command('DUMP', key), replica.execute_command('DUMP', key))

    replica.execute_command('REPLICAOF', 'NO', 'ONE')
    env.assertEqual('master', replica.info('replication')['role'])
    for key in keys:
        for connection in (master, replica):
            connection.execute_command('CMS.INCRBY', key, 'item', 3)
            connection.execute_command('CMS.INCRBY', key, 'item', -1)
        env.assertEqual(master.execute_command('CMS.QUERY', key, 'item'),
                        replica.execute_command('CMS.QUERY', key, 'item'))
        env.assertEqual(master.execute_command('DUMP', key), replica.execute_command('DUMP', key))
