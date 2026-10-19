# MOD-18948: per-write-command primary -> replica data-integrity verification.
#
# For every write command the module registers, run one representative
# invocation on the primary and assert the replica ends up holding the same
# data. Catches commands that are implemented correctly but propagate wrongly
# or not at all (MOD-16050: CF.LOADCHUNK replicated only its header chunk).

import time
from common import *

MODULE_NAME = 'bf'
WAIT_TIMEOUT_MS = 1000


def _exec(con, spec):
    """A spec is either a command arg-list or a callable(con)."""
    return spec(con) if callable(spec) else con.execute_command(*spec)


def _copy_chunks(con, dump_cmd, load_cmd, src, dst):
    """Round-trip every SCANDUMP chunk into dst -- not just the header."""
    it = 0
    while True:
        it, data = con.execute_command(dump_cmd, src, it)
        if it == 0:
            return
        con.execute_command(load_cmd, dst, it, data)


def _bf_loadchunk(con):
    con.execute_command('BF.RESERVE', 'src', '0.01', '1000')
    con.execute_command('BF.MADD', 'src', *[f'i{i}' for i in range(100)])
    _copy_chunks(con, 'BF.SCANDUMP', 'BF.LOADCHUNK', 'src', 'k')


def _cf_loadchunk(con):
    con.execute_command('CF.RESERVE', 'src', '1000')
    for i in range(100):
        con.execute_command('CF.ADD', 'src', f'i{i}')
    _copy_chunks(con, 'CF.SCANDUMP', 'CF.LOADCHUNK', 'src', 'k')


def _cms_sources(con):
    for name in ('s1', 's2'):
        con.execute_command('CMS.INITBYDIM', name, '20', '5')
        con.execute_command('CMS.INCRBY', name, 'a', '5', 'b', '3')
    con.execute_command('CMS.INITBYDIM', 'k', '20', '5')


def _td_sources(con):
    for name in ('s1', 's2'):
        con.execute_command('TDIGEST.CREATE', name, 'COMPRESSION', '100')
        con.execute_command('TDIGEST.ADD', name, '1', '2', '3', '4', '5')


# (command, setup, write-under-test, reads compared on both sides)
#
# One invocation per command, no argument permutations -- but the invocation is
# the form most likely to diverge: autocreate-on-write over a pre-reserved key,
# a scale-out over a single insert, a full chunk round-trip over a header.
ROWS = [
    ('bf.reserve', None, ['BF.RESERVE', 'k', '0.01', '100'],
     [['BF.INFO', 'k']]),
    ('bf.add', None, ['BF.ADD', 'k', 'a'],
     [['BF.EXISTS', 'k', 'a'], ['BF.INFO', 'k']]),
    ('bf.madd', None, ['BF.MADD', 'k', 'a', 'b', 'c'],
     [['BF.MEXISTS', 'k', 'a', 'b', 'c'], ['BF.INFO', 'k']]),
    # CAPACITY 2 forces scaling: the new sub-filters must replicate too.
    ('bf.insert', None, ['BF.INSERT', 'k', 'CAPACITY', '2', 'ITEMS', 'a', 'b', 'c', 'd', 'e'],
     [['BF.MEXISTS', 'k', 'a', 'b', 'c', 'd', 'e'], ['BF.INFO', 'k']]),
    ('bf.loadchunk', None, _bf_loadchunk,
     [['BF.MEXISTS', 'k', 'i0', 'i50', 'i99'], ['BF.INFO', 'k'], ['BF.CARD', 'k']]),

    ('cf.reserve', None, ['CF.RESERVE', 'k', '100', 'BUCKETSIZE', '2'],
     [['CF.INFO', 'k']]),
    ('cf.add', None, ['CF.ADD', 'k', 'a'],
     [['CF.COUNT', 'k', 'a'], ['CF.INFO', 'k']]),
    ('cf.addnx', [['CF.ADD', 'k', 'a']], ['CF.ADDNX', 'k', 'b'],
     [['CF.MEXISTS', 'k', 'a', 'b'], ['CF.INFO', 'k']]),
    ('cf.insert', None, ['CF.INSERT', 'k', 'ITEMS', 'a', 'b', 'c'],
     [['CF.MEXISTS', 'k', 'a', 'b', 'c'], ['CF.INFO', 'k']]),
    ('cf.insertnx', None, ['CF.INSERTNX', 'k', 'ITEMS', 'a', 'b', 'c'],
     [['CF.MEXISTS', 'k', 'a', 'b', 'c'], ['CF.INFO', 'k']]),
    # 'a' inserted twice so the surviving count after DEL is non-trivial.
    ('cf.del', [['CF.ADD', 'k', 'a'], ['CF.ADD', 'k', 'a']], ['CF.DEL', 'k', 'a'],
     [['CF.COUNT', 'k', 'a'], ['CF.INFO', 'k']]),
    ('cf.loadchunk', None, _cf_loadchunk,
     [['CF.MEXISTS', 'k', 'i0', 'i50', 'i99'], ['CF.INFO', 'k']]),

    ('cms.initbydim', None, ['CMS.INITBYDIM', 'k', '20', '5'],
     [['CMS.INFO', 'k']]),
    ('cms.initbyprob', None, ['CMS.INITBYPROB', 'k', '0.001', '0.01'],
     [['CMS.INFO', 'k']]),
    ('cms.incrby', [['CMS.INITBYDIM', 'k', '20', '5']], ['CMS.INCRBY', 'k', 'a', '5', 'b', '3'],
     [['CMS.QUERY', 'k', 'a', 'b'], ['CMS.INFO', 'k']]),
    ('cms.merge', _cms_sources, ['CMS.MERGE', 'k', '2', 's1', 's2'],
     [['CMS.QUERY', 'k', 'a', 'b'], ['CMS.INFO', 'k']]),

    ('topk.reserve', None, ['TOPK.RESERVE', 'k', '3', '20', '5', '0.9'],
     [['TOPK.INFO', 'k']]),
    ('topk.add', [['TOPK.RESERVE', 'k', '3', '20', '5', '0.9']], ['TOPK.ADD', 'k', 'a', 'b', 'c'],
     [['TOPK.LIST', 'k', 'WITHCOUNT'], ['TOPK.INFO', 'k']]),
    ('topk.incrby', [['TOPK.RESERVE', 'k', '3', '20', '5', '0.9'], ['TOPK.ADD', 'k', 'a']],
     ['TOPK.INCRBY', 'k', 'a', '5'],
     [['TOPK.LIST', 'k', 'WITHCOUNT'], ['TOPK.COUNT', 'k', 'a']]),

    ('tdigest.create', None, ['TDIGEST.CREATE', 'k', 'COMPRESSION', '100'],
     [['TDIGEST.INFO', 'k']]),
    ('tdigest.add', [['TDIGEST.CREATE', 'k', 'COMPRESSION', '100']],
     ['TDIGEST.ADD', 'k', '1', '2', '3', '4', '5'],
     [['TDIGEST.QUANTILE', 'k', '0.5'], ['TDIGEST.INFO', 'k']]),
    ('tdigest.reset', [['TDIGEST.CREATE', 'k', 'COMPRESSION', '100'],
                       ['TDIGEST.ADD', 'k', '1', '2', '3']], ['TDIGEST.RESET', 'k'],
     [['TDIGEST.INFO', 'k']]),
    ('tdigest.merge', _td_sources, ['TDIGEST.MERGE', 'k', '2', 's1', 's2'],
     [['TDIGEST.QUANTILE', 'k', '0.5'], ['TDIGEST.INFO', 'k']]),
]

# Write commands deliberately not in ROWS, with the reason. Keep this empty.
KNOWN_EXCLUSIONS = set()

# HeavyKeeper decays counters with an unseeded rand() (src/topk.c:173), so the
# replica's internal layout legitimately differs after re-executing the same
# command. Only the value oracle is meaningful for these; the key set still is.
NO_DUMP_COMPARE = {'topk.add', 'topk.incrby'}


def _wait_link_up(env, timeout=10):
    """RLTest starts the replica with --slaveof but never waits for the sync."""
    slave = env.getSlaveConnection()
    deadline = time.time() + timeout
    while time.time() < deadline:
        if slave.execute_command('INFO', 'replication')['master_link_status'] == 'up':
            return
        time.sleep(0.1)
    env.assertTrue(False, message='replica link never came up')


def _is_trivial(val):
    """A read that returns the type default proves nothing -- both sides match."""
    if val is None or val == 0 or val == b'' or val == '':
        return True
    if isinstance(val, (list, tuple)):
        return len(val) == 0 or all(_is_trivial(v) for v in val)
    return False


def _read_slave(con, spec):
    """A divergence must be reported as a diff, not raised -- keep rows independent."""
    try:
        return con.execute_command(*spec)
    except ResponseError as e:
        return f'error: {e}'


def _keys(con):
    return sorted(con.execute_command('KEYS', '*'))


def _dumps(con, keys):
    return [con.execute_command('DUMP', k) for k in keys]


def _verify_row(env, master, slave, cmd, setup, write, reads):
    master.execute_command('FLUSHALL')
    if setup is not None:
        for spec in (setup if isinstance(setup, list) else [setup]):
            _exec(master, spec)
    _exec(master, write)

    acked = master.execute_command('WAIT', 1, WAIT_TIMEOUT_MS)
    env.assertEqual(acked, 1, message=f'{cmd}: replica did not ack the write')

    # Snapshot before reading: some reads (TDIGEST.QUANTILE) compact in place.
    keys = _keys(master)
    env.assertEqual(keys, _keys(slave), message=f'{cmd}: key set diverged')
    if cmd not in NO_DUMP_COMPARE:
        env.assertEqual(_dumps(master, keys), _dumps(slave, keys),
                        message=f'{cmd}: DUMP diverged')

    for spec in reads:
        got = master.execute_command(*spec)
        env.assertFalse(_is_trivial(got), message=f'{cmd}: master {spec[0]} returned a default value')
        env.assertEqual(got, _read_slave(slave, spec), message=f'{cmd}: {spec[0]} diverged')


def testWriteCommandsReplicate(env):
    env.skipOnCluster()  # a cluster env has no replica connection to read
    # Under --use-aof the replica below would run with AOF, and RLTest intermittently hangs
    # stopping such a replica (it ignores SIGTERM; RLTest then waits on it with no timeout).
    # Replication is covered by the other env groups, so skip rather than add it here.
    env.skipOnAOF()
    env = Env(useSlaves=True, protocol=2)
    master, slave = env.getConnection(), env.getSlaveConnection()
    _wait_link_up(env)
    for cmd, setup, write, reads in ROWS:
        _verify_row(env, master, slave, cmd, setup, write, reads)


def testEveryWriteCommandIsCovered(env):
    """The command table is only as good as its coverage of the real command set."""
    env = Env(protocol=2)  # no replica needed: this only reads the command table
    if server_version_less_than(env, '7.0'):
        env.skip()
    con = env.getConnection()
    # redis-py pipes every COMMAND * reply through its COMMAND INFO parser,
    # which cannot read a COMMAND LIST reply. Take the raw replies instead.
    con.set_response_callback('COMMAND', lambda r, **_: r)
    names = con.execute_command('COMMAND', 'LIST', 'FILTERBY', 'MODULE', MODULE_NAME)
    info = con.execute_command('COMMAND', 'INFO', *names)
    write_cmds = {c[0].decode().lower() for c in info if c and b'write' in c[2]}
    missing = write_cmds - {cmd for cmd, _, _, _ in ROWS} - KNOWN_EXCLUSIONS
    env.assertEqual(missing, set(), message=f'write commands with no replication test: {missing}')


def testTopKReplicationCommandVersion(env):
    env.skipOnCluster()
    env.skipOnAOF()
    env = Env(useSlaves=True, protocol=2)
    master, slave = env.getConnection(), env.getSlaveConnection()
    _wait_link_up(env)
    master.execute_command('TOPK.RESERVE', 'k', 3, 5, 3, 0.9)
    master.execute_command('TOPK.ADD', 'k', 'item', 'item')
    master.execute_command('TOPK.INCRBY', 'k', 'item', 3)
    env.assertEqual(master.execute_command('WAIT', 1, WAIT_TIMEOUT_MS), 1)
    env.assertEqual(slave.execute_command('TOPK.COUNT', 'k', 'item'), [5])
    stats = slave.info('commandstats')
    for command in ('add', 'incrby'):
        public = f'cmdstat_topk.{command}'
        seeded = f'cmdstat_topk._{command}_seeded'
        expected, absent = ((seeded, public) if server_version_at_least(env, '8.0.0')
                            else (public, seeded))
        env.assertEqual(stats.get(expected, {}).get('calls', 0), 1)
        env.assertEqual(stats.get(absent, {}).get('calls', 0), 0)
    env.assertEqual(slave.info('stats')['unexpected_error_replies'], 0)


def testTopKDecayReplicates(env):
    env.skipOnCluster()
    env.skipOnAOF()
    env = Env(useSlaves=True, protocol=2)
    if server_version_less_than(env, '8.0.0'):
        env.skip()
    master, slave = env.getConnection(), env.getSlaveConnection()
    _wait_link_up(env)
    items = [f'item-{i}' for i in range(20)]
    master.execute_command('FUNCTION', 'LOAD', '''#!lua name=topk_replay
redis.register_function('topk_replay', function(keys, args)
    return redis.call(args[1], keys[1], unpack(args, 2))
end)''')
    for command in ('TOPK.ADD', 'TOPK.INCRBY'):
        master.execute_command('FLUSHALL')
        # A narrow sketch forces collisions and probabilistic counter decay.
        master.execute_command('TOPK.RESERVE', 'k', 3, 5, 3, 0.9)
        for batch in range(10):
            mode = ('direct', 'transaction', 'eval', 'fcall')[batch % 4]
            with master.pipeline(transaction=mode == 'transaction') as pipeline:
                for i in range(100):
                    item = items[(i * 7 + batch) % len(items)]
                    args = [item] if command == 'TOPK.ADD' else [item, i % 5 + 1]
                    if mode == 'eval':
                        pipeline.execute_command('EVAL',
                            'return redis.call(ARGV[1], KEYS[1], unpack(ARGV, 2))',
                            1, 'k', command, *args)
                    elif mode == 'fcall':
                        pipeline.execute_command('FCALL', 'topk_replay', 1, 'k', command, *args)
                    else:
                        pipeline.execute_command(command, 'k', *args)
                pipeline.execute()
            env.assertEqual(master.execute_command('WAIT', 1, WAIT_TIMEOUT_MS), 1)
            for read, args in (('TOPK.LIST', ['WITHCOUNT']),
                               ('TOPK.QUERY', items), ('TOPK.COUNT', items)):
                env.assertEqual(master.execute_command(read, 'k', *args),
                                slave.execute_command(read, 'k', *args),
                                message=f'{command} {mode} batch {batch}: {read} diverged')
            if batch == 4:
                # Force a full sync of a populated sketch, then continue the same workload.
                syncs = master.execute_command('INFO', 'stats')['sync_full']
                env.stop(masters=False)
                master.execute_command('DEBUG', 'CHANGE-REPL-ID')
                env.start(masters=False)
                slave = env.getSlaveConnection()
                _wait_link_up(env)
                env.assertEqual(master.execute_command('INFO', 'stats')['sync_full'], syncs + 1)


def testTopKIncrbyPartialFailureReplicates(env):
    env.skipOnCluster()
    env.skipOnAOF()
    env = Env(useSlaves=True, protocol=2)
    master, slave = env.getConnection(), env.getSlaveConnection()
    _wait_link_up(env)
    master.execute_command('TOPK.RESERVE', 'partial', 3, 5, 3, 0.9)
    env.assertEqual(master.execute_command('WAIT', 1, WAIT_TIMEOUT_MS), 1)
    errors = slave.info('stats')['unexpected_error_replies']
    for args in (('invalid', -1), ('a', 5, 'invalid', -1)):
        reply = master.execute_command('TOPK.INCRBY', 'partial', *args)
        env.assertTrue(isinstance(reply[-1], ResponseError))
        env.assertEqual(master.execute_command('WAIT', 1, WAIT_TIMEOUT_MS), 1)
        env.assertEqual(slave.info('stats')['unexpected_error_replies'], errors)
        if len(args) == 2:
            command = ('topk._incrby_seeded' if server_version_at_least(env, '8.0.0')
                       else 'topk.incrby')
            env.assertEqual(slave.info('commandstats').get(f'cmdstat_{command}', {})
                            .get('calls', 0), 0)
        env.assertEqual(master.execute_command('TOPK.LIST', 'partial', 'WITHCOUNT'),
                        slave.execute_command('TOPK.LIST', 'partial', 'WITHCOUNT'))
