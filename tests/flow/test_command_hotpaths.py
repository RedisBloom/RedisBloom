from common import Env, server_version_less_than


def check_reply_types(env, protocol):
    boolean_type = bool if protocol == 3 else int

    def boolean_reply(command, expected):
        reply = env.cmd(*command)
        env.assertEqual(reply, expected)
        for item in reply if isinstance(reply, list) else [reply]:
            env.assertEqual(type(item), boolean_type)

    env.cmd('SET', 'wrong-type', 'value')
    for family in ('BF', 'CF'):
        for key in ('missing', 'wrong-type'):
            boolean_reply([family + '.EXISTS', key, 'a'], False)
            boolean_reply([family + '.MEXISTS', key, 'a', 'b'], [False, False])
        boolean_reply([family + '.ADD', family, 'a'], True)
        boolean_reply([family + '.EXISTS', family, 'a'], True)
        boolean_reply([family + '.MEXISTS', family, 'a', 'b'], [True, False])
        env.expect(family + '.ADD', 'wrong-type', 'a').error()
        env.expect(family + '.INSERT', 'missing', 'NOCREATE', 'ITEMS', 'a').error()
        env.assertEqual(env.cmd('EXISTS', 'missing'), 0)

    boolean_reply(['BF.MADD', 'BF', 'a', 'b', 'b'], [False, True, False])
    boolean_reply(['BF.INSERT', 'BF', 'ITEMS', 'a', 'c'], [False, True])
    boolean_reply(['CF.ADDNX', 'CF', 'a'], False)
    reply = env.cmd('CF.INSERTNX', 'CF', 'ITEMS', 'a', 'b')
    env.assertEqual(reply, [0, 1])
    env.assertTrue(all(type(item) is int for item in reply))
    for key, expected in (('missing', 0), ('wrong-type', 0), ('CF', 1)):
        reply = env.cmd('CF.COUNT', key, 'a')
        env.assertEqual(reply, expected)
        env.assertEqual(type(reply), int)

    # CF.INSERT failure is -1 in RESP2 but false in RESP3. CF.INSERTNX
    # retains its integer failure even on a RESP3 connection.
    items = ['item:%d' % i for i in range(128)]
    for command in ('CF.INSERT', 'CF.INSERTNX'):
        env.cmd('CF.RESERVE', command, 2, 'BUCKETSIZE', 1, 'EXPANSION', 0)
        reply = env.cmd(command, command, 'ITEMS', *items)
        reply_type = boolean_type if command == 'CF.INSERT' else int
        failure = False if command == 'CF.INSERT' and protocol == 3 else -1
        env.assertTrue(all(type(item) is reply_type for item in reply))
        env.assertTrue(failure in reply)
        env.expect('CF.ADD', command, 'full').error().contains('Filter is full')


def test_reply_types_resp2():
    check_reply_types(Env(protocol=2), 2)


def test_reply_types_resp3():
    env = Env(protocol=3)
    if server_version_less_than(env, '7.0.0'):
        env.skip()
    check_reply_types(env, 3)


def test_cms_single_and_multi_increment_cleanup():
    env = Env()
    env.cmd('SET', 'wrong-type', 'value')
    env.cmd('CMS.INITBYDIM', 'cms', 100, 5)
    # Exercise the explicit key cleanup on errors as well as both parse buffers.
    for _ in range(32):
        for key in ('missing', 'wrong-type'):
            env.expect('CMS.QUERY', key, 'a').error()
            env.expect('CMS.INCRBY', key, 'a', 1).error()
        for value in ('invalid', '-9223372036854775808'):
            env.expect('CMS.INCRBY', 'cms', 'a', value).error()
            env.expect('CMS.INCRBY', 'cms', 'a', 1, 'b', value).error()
    env.assertEqual(env.cmd('CMS.QUERY', 'cms', 'a', 'b'), [0, 0])
    env.assertEqual(env.cmd('CMS.INCRBY', 'cms', 'a', 2), [2])
    env.assertEqual(env.cmd('CMS.INCRBY', 'cms', 'a', -1, 'b', 3), [1, 3])
    env.assertEqual(env.cmd('CMS.QUERY', 'cms', 'a', 'b'), [1, 3])
