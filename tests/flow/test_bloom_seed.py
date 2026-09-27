from common import Env


def test_reserve_seed():
    env = Env(decodeResponses=False)
    if env.useSlaves:
        # Exercise command propagation; seeded RDB/full-sync support is a later step.
        env.cmd('SET', 'seed-test-sync', 'ready')
        env.assertEqual(1, env.cmd('WAIT', 1, 10000))
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
