from common import Env
from rdb_corruption_utils import crc64_redis, encode_len, load_len, rewrite_module_uint
from test_bloom_seed import wait_until


SEED_VALUES = (None, 0, 123, 0x80000000, 0xffffffff, 0xffffffff - 1918, 'random')


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
        for value in SEED_VALUES:
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
    cases = []
    for params in ([], [64, 5, 0.9]):
        env.cmd('DEL', 'template')
        env.cmd('TOPK.RESERVE', 'template', 3, *params)
        template = env.cmd('DUMP', 'template')
        env.assertEqual(1, load_len(template, 1)[0] & 1023)
        for seed in SEED_VALUES:
            for index, item in enumerate((None, b'', b'item\xff')):
                key = f'{len(params)}-{seed}-{index}'
                options = [] if seed is None else ['SEED', seed]
                env.cmd('TOPK.RESERVE', key, 3, *params, *options)
                payload = env.cmd('DUMP', key)
                # Empty sketches differ only in their final UINT seed record.
                # Capture it before insertion; populated heaps contain raw pointers.
                env.assertEqual(template[:-13], payload[:len(template) - 13])
                suffix = payload[len(template) - 13:-10]
                if seed != 'random':
                    env.assertEqual(b'\x02' + encode_len(seed or 0) + b'\x00', suffix)
                if item is not None:
                    env.cmd('TOPK.INCRBY', key, item, 7)
                env.cmd('RESTORE', key + '-copy', 0, env.cmd('DUMP', key))
                cases.extend([(key, suffix, item), (key + '-copy', suffix, item)])
    for command in ([None, 'AOF'] if env.useAof else [None, 'SAVE', 'BGSAVE']):
        if command == 'AOF':
            env.dumpAndReload(restart=True)
        elif command:
            env.cmd('CONFIG', 'SET', 'save', '')
            env.cmd(command)
            wait_until(env, lambda: not env.cmd('INFO', 'persistence')['rdb_bgsave_in_progress'])
            env.assertEqual('ok', env.cmd('INFO', 'persistence')['rdb_last_bgsave_status'])
            env.cmd('SET', 'after-snapshot', 'must-not-survive')
            env.stop()
            env.start()
            env.assertEqual(0, env.cmd('EXISTS', 'after-snapshot'))
        for key, suffix, item in cases:
            env.assertTrue(env.cmd('DUMP', key)[:-10].endswith(suffix))
            env.assertEqual([0 if item is None else 7], env.cmd('TOPK.COUNT', key, item or b''))
            env.assertEqual([0 if item is None else 1], env.cmd('TOPK.QUERY', key, item or b''))
            env.assertEqual([] if item is None else [item, 7], env.cmd('TOPK.LIST', key, 'WITHCOUNT'))
    for key, _, item in cases:
        env.cmd('TOPK.INCRBY', key, item or b'', 3)
        env.assertEqual([3 if item is None else 10], env.cmd('TOPK.COUNT', key, item or b''))
        env.assertEqual([1], env.cmd('TOPK.QUERY', key, item or b''))


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


def test_seed_replica_recovery():
    env = Env(useSlaves=True, decodeResponses=False, freshEnv=True)
    env.skipOnCluster()
    master, replica = env.getConnection(), env.getSlaveConnection()
    cases = []
    for params in ([], [64, 5, 0.9]):
        for seed in SEED_VALUES:
            for populated in (False, True):
                key = f'{len(params)}-{seed}-{populated}'
                options = [] if seed is None else ['SEED', seed]
                env.cmd('TOPK.RESERVE', key, 3, *params, *options)
                empty_dump = env.cmd('DUMP', key)
                if populated:
                    env.cmd('TOPK.INCRBY', key, '', 7)
                cases.append((key, empty_dump, populated))

    def check_replica():
        wait_until(env, lambda: master.execute_command('WAIT', 1, 1000) == 1)
        for key, empty_dump, populated in cases:
            if not populated:
                env.assertEqual(empty_dump, replica.execute_command('DUMP', key))
            for command in ('TOPK.COUNT', 'TOPK.QUERY'):
                env.assertEqual(master.execute_command(command, key, ''),
                                replica.execute_command(command, key, ''))
            env.assertEqual(master.execute_command('TOPK.LIST', key, 'WITHCOUNT'),
                            replica.execute_command('TOPK.LIST', key, 'WITHCOUNT'))

    check_replica()
    master.execute_command('CLIENT', 'KILL', 'TYPE', 'replica')
    master.execute_command('TOPK.INCRBY', cases[-1][0], '', 1)
    check_replica()

    replication = replica.info('replication')
    full_syncs = master.info('stats')['sync_full']
    replica.execute_command('REPLICAOF', 'NO', 'ONE')
    replica.execute_command('FLUSHALL')
    replica.execute_command('REPLICAOF', replication['master_host'], replication['master_port'])
    wait_until(env, lambda: master.info('stats')['sync_full'] > full_syncs)
    check_replica()

    replica.execute_command('REPLICAOF', 'NO', 'ONE')
    env.assertEqual('master', replica.info('replication')['role'])
    for key, _, _ in cases:
        # One item per sketch isolates seed recovery from random counter decay.
        for connection in (master, replica):
            connection.execute_command('TOPK.INCRBY', key, '', 3)
        env.assertEqual(master.execute_command('TOPK.COUNT', key, ''),
                        replica.execute_command('TOPK.COUNT', key, ''))
        env.assertEqual([1], replica.execute_command('TOPK.QUERY', key, ''))
