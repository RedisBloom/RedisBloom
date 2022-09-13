
from common import *


def ConvertInfo(lst):
    res_dct = {lst[i]: lst[i + 1] for i in range(0, len(lst), 2)}
    return res_dct


class testRedisBloom():
    def __init__(self):
        self.env = Env(decodeResponses=True)

    def test_custom_filter(self):
        env = self.env
        env.cmd('FLUSHALL')
        env.assertOk(env.cmd('bf.reserve', 'test', '0.05', '1000'))
        env.assertEqual([1, 1, 1], env.cmd(
            'bf.madd', 'test', 'foo', 'bar', 'baz'))
        # TODO: re-enable this portion after RLTest migration
        # env.retry_with_rdb_reload()

        env.assertEqual(1, env.cmd('bf.exists', 'test', 'foo'))
        env.assertEqual(0, env.cmd('bf.exists', 'test', 'nonexist'))

        with env.assertResponseError():
            env.cmd('bf.reserve', 'test', '0.01')

        # TODO: re-enable this portion after RLTest migration
        # # Ensure there's no error when trying to add elements to it:
        # env.retry_with_rdb_reload()
        # env.assertEqual(0, env.cmd('bf.add', 'test', 'foo'))

    def test_set(self):
        env = self.env
        env.cmd('FLUSHALL')
        env.assertEqual(1, env.cmd('bf.add', 'test', 'foo'))
        env.assertEqual(1, env.cmd('bf.exists', 'test', 'foo'))
        env.assertEqual(0, env.cmd('bf.exists', 'test', 'bar'))

        # Set multiple keys at once:
        env.assertEqual([0, 1], env.cmd('bf.madd', 'test', 'foo', 'bar'))

        # TODO: re-enable this portion after RLTest migration
        # env.retry_with_rdb_reload()
        env.assertEqual(1, env.cmd('bf.exists', 'test', 'foo'))
        env.assertEqual(1, env.cmd('bf.exists', 'test', 'bar'))
        env.assertEqual(0, env.cmd('bf.exists', 'test', 'nonexist'))

    def test_multi(self):
        env = self.env
        env.cmd('FLUSHALL')
        env.assertEqual([1, 1, 1], env.cmd(
            'bf.madd', 'test', 'foo', 'bar', 'baz'))
        env.assertEqual([0, 1, 0], env.cmd(
            'bf.madd', 'test', 'foo', 'new1', 'bar'))

        env.assertEqual([0], env.cmd('bf.mexists', 'test', 'nonexist'))

        env.assertEqual([0, 1], env.cmd(
            'bf.mexists', 'test', 'nonexist', 'foo'))

    def test_validation(self):
        env = self.env
        env.cmd('FLUSHALL')
        for args in (
                (),
                ('foo',),
                ('foo', '0.001'),
                ('foo', '0.001', 'blah'),
                ('foo', '0', '0'),
                ('foo', '0', '100'),
                ('foo', 'blah', '1000'),
                ('foo', '7.7', '1000')
        ):
            env.assertRaises(ResponseError, env.cmd, 'bf.reserve', *args)

        env.assertOk(env.cmd('bf.reserve', 'test', '0.01', '1000'))
        for args in ((), ('test',)):
            for cmd in ('bf.add', 'bf.madd', 'bf.exists', 'bf.mexists'):
                env.assertRaises(ResponseError, env.cmd, cmd, *args)

        for cmd in ('bf.exists', 'bf.add'):
            env.assertRaises(ResponseError, env.cmd, cmd, 'test', 1, 2)

        env.cmd('set', 'foo', 'bar')
        env.assertRaises(ResponseError, env.cmd, 'bf.reserve', 'foo')

    def test_oom(self):
        env = self.env
        env.cmd('FLUSHALL')
        env.assertRaises(ResponseError, env.cmd, 'bf.reserve', 'test', 0.01, 4294967296 * 4294967296)

    def test_rdb_reload(self):
        env = self.env
        env.cmd('FLUSHALL')
        env.assertEqual(1, env.cmd('bf.add', 'test', 'foo'))
        yield 1
        self.env.dumpAndReload()
        yield 2
        env.assertEqual(1, env.cmd('bf.exists', 'test', 'foo'))
        env.assertEqual(0, env.cmd('bf.exists', 'test', 'bar'))

    def test_dump_and_load(self):
        env = self.env
        env.cmd('FLUSHALL')
        # Store a filter
        quantity = 1000
        error_rate = 0.001
        env.cmd('bf.reserve', 'myBloom', error_rate, quantity * 1024 * 8)

        # test is probabilistic and might fail. It is OK to change variables if
        # certain to not break anything
        def do_verify(add):
            false_positives = 0.0
            for x in range(quantity):
                if add:
                    env.cmd('bf.add', 'myBloom', x)
                rv = env.cmd('bf.exists', 'myBloom', x)
                env.assertTrue(rv)
                rv = env.cmd('bf.exists', 'myBloom', 'nonexist_{}'.format(x))
                if rv == 1:
                    false_positives += 1
            env.assertLessEqual(false_positives / quantity, error_rate)

        with env.assertResponseError():
            env.cmd('bf.scandump', 'myBloom')
        with env.assertResponseError():
            env.cmd('bf.scandump', 'myBloom', 'str')
        with env.assertResponseError():
            env.cmd('bf.scandump', 'myCuckoo', 0)
        with env.assertResponseError():
            env.cmd('bf.loadchunk', 'myBloom')
        with env.assertResponseError():
            env.cmd('bf.loadchunk', 'myBloom', 'str', 'data')

            # TODO: Enable this portion again after RLTest migration
        # do_verify(add=True)
        # cmds = []
        # cur = env.cmd('bf.scandump', 'myBloom', 0)
        # first = cur[0]
        # cmds.append(cur)

        # while True:
        #     cur = env.cmd('bf.scandump', 'myBloom', first)
        #     first = cur[0]
        #     if first == 0:
        #         break
        #     else:
        #         cmds.append(cur)
        #         print("Scaning chunk... (P={}. Len={})".format(cur[0], len(cur[1])))

        # prev_info = env.cmd('bf.debug', 'myBloom')
        # # Remove the filter
        # env.cmd('del', 'myBloom')

        # # Now, load all the commands:
        # for cmd in cmds:
        #     print("Loading chunk... (P={}. Len={})".format(cmd[0], len(cmd[1])))
        #     env.cmd('bf.loadchunk', 'myBloom', *cmd)

        # cur_info = env.cmd('bf.debug', 'myBloom')
        # env.assertEqual(prev_info, cur_info)
        # do_verify(add=False)

        # # Try a bigger one
        # env.cmd('del', 'myBloom')
        # env.cmd('bf.reserve', 'myBloom', '0.0001', '10000000')

    def test_missing(self):
        env = self.env
        env.cmd('FLUSHALL')
        res = env.cmd('bf.exists', 'myBloom', 'foo')
        env.assertEqual(0, res)
        res = env.cmd('bf.mexists', 'myBloom', 'foo', 'bar', 'baz')
        env.assertEqual([0, 0, 0], res)

    def test_insert(self):
        env = self.env
        env.cmd('FLUSHALL')
        with env.assertResponseError():
            env.cmd('bf.insert', 'missingFilter', 'NOCREATE', 'ITEMS', 'foo', 'bar')
        with env.assertResponseError():
            env.cmd('bf.insert', 'missingFilter', 'DONTEXIST', 'NOCREATE')
        with env.assertResponseError():
            env.cmd('bf.insert', 'missingFilter')
        with env.assertResponseError():
            env.cmd('bf.insert', 'missingFilter', 'NOCREATE', 'ITEMS')
        with env.assertResponseError():
            env.cmd('bf.insert', 'missingFilter', 'NOCREATE', 'CAPACITY')
        with env.assertResponseError():
            env.cmd('bf.insert', 'missingFilter', 'CAPACITY', 0.3)
        with env.assertResponseError():
            env.cmd('bf.insert', 'missingFilter', 'ERROR')
        with env.assertResponseError():
            env.cmd('bf.insert', 'missingFilter', 'ERROR', 'big')
        with env.assertResponseError():
            env.cmd('bf.insert', 'missingFilter', 'EXPANSION', '0', 'ITEMS', 'foo')
        with env.assertResponseError():
            env.cmd('bf.insert', 'missingFilter', 'EXPANSION', 'big')

        rep = env.cmd('BF.INSERT', 'missingFilter', 'ERROR',
                       '0.001', 'CAPACITY', '50000', 'EXPANSION', 2, 'ITEMS', 'foo')
        env.assertEqual([1], rep)
        env.assertEqual(
            ['size:1', 'bytes:98880 bits:791040 hashes:11 hashwidth:64 capacity:50000 size:1 ratio:0.0005'],
            [x for x in env.cmd('bf.debug', 'missingFilter')])

        rep = env.cmd('BF.INSERT', 'missingFilter', 'ERROR', '0.1', 'ITEMS', 'foo', 'bar', 'baz')
        env.assertEqual([0, 1, 1], rep)
        env.assertEqual(
            ['size:3', 'bytes:98880 bits:791040 hashes:11 hashwidth:64 capacity:50000 size:3 ratio:0.0005'],
            [x for x in env.cmd('bf.debug', 'missingFilter')])

    def test_mem_usage(self):
        env = self.env
        env.cmd('FLUSHALL')
        env.assertOk(env.cmd('bf.reserve', 'bf', '0.05', '1000'))
        if not VALGRIND:
            env.assertEqual(1088, env.cmd('MEMORY USAGE', 'bf'))
        env.assertEqual([1, 1, 1], env.cmd(
            'bf.madd', 'bf', 'foo', 'bar', 'baz'))
        if not VALGRIND:
            env.assertEqual(1088, env.cmd('MEMORY USAGE', 'bf'))
        with env.assertResponseError():
            env.cmd('bf.debug', 'bf', 'noexist')
        with env.assertResponseError():
            env.cmd('bf.debug', 'cf')

    def test_expansion(self):
        env = self.env
        env.cmd('FLUSHALL')
        env.assertOk(env.cmd('bf.reserve exp1 0.01 4 expansion 1'))
        env.assertOk(env.cmd('bf.reserve exp2 0.01 4 expansion 2'))
        env.assertOk(env.cmd('bf.reserve exp4 0.01 4 expansion 4'))
        for i in range(100):
            env.cmd('bf.add exp1', str(i))
            env.cmd('bf.add exp2', str(i))
            env.cmd('bf.add exp4', str(i))
        for i in range(100):
            env.assertEqual(1, env.cmd('bf.exists exp1', str(i)))
            env.assertEqual(1, env.cmd('bf.exists exp2', str(i)))
            env.assertEqual(1, env.cmd('bf.exists exp4', str(i)))

        env.assertEqual(23, env.cmd('bf.info', 'exp1')[5])
        env.assertEqual(5, env.cmd('bf.info', 'exp2')[5])
        env.assertEqual(4, env.cmd('bf.info', 'exp4')[5])

        with env.assertResponseError():
            env.cmd('bf.reserve exp4 0.01 4 expansion')
        with env.assertResponseError():
            env.cmd('bf.reserve exp4 0.01 4 expansion 0')
        with env.assertResponseError():
            env.cmd('bf.reserve exp4 0.01 4 expansion str')

    def test_debug(self):
        env = self.env
        env.cmd('FLUSHALL')
        env.assertOk(env.cmd('bf.reserve', 'bf', '0.01', '10'))
        for i in range(100):
            env.cmd('bf.add', 'bf', str(i))
        env.assertEqual(env.cmd('bf.debug', 'bf'), ['size:99',
                                                      'bytes:16 bits:128 hashes:8 hashwidth:64 capacity:10 size:10 ratio:0.005',
                                                      'bytes:32 bits:256 hashes:9 hashwidth:64 capacity:20 size:20 ratio:0.0025',
                                                      'bytes:72 bits:576 hashes:10 hashwidth:64 capacity:40 size:40 ratio:0.00125',
                                                      'bytes:160 bits:1280 hashes:11 hashwidth:64 capacity:80 size:29 ratio:0.000625'])

        env.cmd('del', 'bf')

        env.assertOk(env.cmd('bf.reserve', 'bf', '0.001', '100'))
        for i in range(4000):
            env.cmd('bf.add', 'bf', str(i))
        env.assertEqual(env.cmd('bf.debug', 'bf'), ['size:3997',
                                                      'bytes:200 bits:1600 hashes:11 hashwidth:64 capacity:100 size:100 ratio:0.0005',
                                                      'bytes:432 bits:3456 hashes:12 hashwidth:64 capacity:200 size:200 ratio:0.00025',
                                                      'bytes:936 bits:7488 hashes:13 hashwidth:64 capacity:400 size:400 ratio:0.000125',
                                                      'bytes:2016 bits:16128 hashes:14 hashwidth:64 capacity:800 size:800 ratio:6.25e-05',
                                                      'bytes:4320 bits:34560 hashes:15 hashwidth:64 capacity:1600 size:1600 ratio:3.125e-05',
                                                      'bytes:9216 bits:73728 hashes:16 hashwidth:64 capacity:3200 size:897 ratio:1.5625e-05'])

    def test_info(self):
        env = self.env
        env.cmd('FLUSHALL')
        env.assertOk(env.cmd('bf.reserve', 'bf', '0.001', '100'))
        env.assertEqual(env.cmd('bf.info bf'), ['Capacity', 100,
                                                  'Size', 296,
                                                  'Number of filters', 1,
                                                  'Number of items inserted', 0,
                                                  'Expansion rate', 2])

        with env.assertResponseError():
            env.cmd('bf.info', 'cf')
        with env.assertResponseError():
            env.cmd('bf.info')

    def test_info_capacity(self):
        self.cmd('FLUSHALL')
        self.assertOk(self.cmd('bf.reserve', 'bf', '0.001', '100'))
        self.assertEqual(self.cmd('bf.info bf capacity'), [100])

    def test_info_size(self):
        self.cmd('FLUSHALL')
        self.assertOk(self.cmd('bf.reserve', 'bf', '0.001', '100'))
        self.assertEqual(self.cmd('bf.info bf size'), [296])

    def test_info_filters(self):
        self.cmd('FLUSHALL')
        self.assertOk(self.cmd('bf.reserve', 'bf', '0.001', '100'))
        self.assertEqual(self.cmd('bf.info bf filters'), [1])

    def test_info_items(self):
        self.cmd('FLUSHALL')
        self.assertOk(self.cmd('bf.reserve', 'bf', '0.001', '100'))
        self.assertEqual(self.cmd('bf.info bf items'), [0])

    def test_info_expansion(self):
        self.cmd('FLUSHALL')
        self.assertOk(self.cmd('bf.reserve', 'bf', '0.001', '100'))
        self.assertEqual(self.cmd('bf.info bf expansion'), [2])

    def test_info_errors(self):
        self.cmd('FLUSHALL')
        self.assertOk(self.cmd('bf.reserve', 'bf', '0.001', '100'))
        with self.assertResponseError():
            self.cmd('bf.info', 'bf', 'capacity', 'size')
        with self.assertResponseError():
            self.cmd('bf.info', 'bf', 'wrong_value')

    def test_no_1_error_rate(self):
        env = self.env
        env.cmd('FLUSHALL')
        with env.assertResponseError():
            env.cmd('bf.reserve bf 1 1000')

    def test_error_rate(self):
        env = self.env
        env.cmd('FLUSHALL')
        repeat = 1024
        rates = [0.1, 0.01, 0.001, 0.0001]
        names = ['bf0.1', 'bf0.01', 'bf0.001', 'bf0.0001']

        for i in range(len(rates)):
            false_positive = 0.0
            env.cmd('bf.reserve', names[i], rates[i], repeat)
            for x in range(repeat):
                env.cmd('bf.add', names[i], x)
            for x in range(repeat, repeat * 11):
                false_positive += env.cmd('bf.exists', names[i], x)
            env.assertGreaterEqual(rates[i], false_positive / (repeat * 10))

    def test_no_scaling(self):
        env = self.env
        env.cmd('FLUSHALL')
        capacity = 3
        env.assertOk(env.cmd('bf.reserve bf 0.01', capacity, 'nonscaling'))
        for i in range(capacity):
            env.assertEqual(1, env.cmd('bf.add bf', i))
        with env.assertResponseError():
            env.cmd('bf.add bf extra')
        with env.assertResponseError():
            env.cmd('bf.reserve bf_mix 0.01 1000 nonscaling expansion 2')

        env.assertOk(env.cmd('bf.reserve bfnonscale 0.001 1000 nonscaling'))
        env.assertOk(env.cmd('bf.reserve bfscale 0.001 1000'))
        env.assertLess(env.cmd('bf.info bfnonscale')[3], env.cmd('bf.info bfscale')[3])

    def test_nonscaling_err(self):
        env = self.env
        env.cmd('FLUSHALL')
        capacity = 3
        env.assertEqual([1, 1, 1], env.cmd('BF.INSERT nonscaling_err CAPACITY 3 NONSCALING ITEMS a b c'))
        resp = env.cmd('BF.INSERT nonscaling_err ITEMS a b c d d')
        env.assertEqual([0, 0, 0, ], resp[:3])
        env.assertEqual('non scaling filter is full', str(resp[3]))
        info_actual = env.cmd('BF.INFO nonscaling_err')
        info_expected = ['Capacity', 3, 'Size', 104, 'Number of filters', 1,
                         'Number of items inserted', 3, 'Expansion rate', None]
        env.assertEqual(info_actual, info_expected)

    def test_issue178(self):
        env = self.env
        env.cmd('FLUSHALL')
        capacity = 300 * 1000 * 1000
        error_rate = 0.000001
        env.assertOk(env.cmd('bf.reserve bf', error_rate, capacity))
        info = ConvertInfo(env.cmd('bf.info bf'))
        env.assertEqual(info["Capacity"], 300000000)
        env.assertEqual(info["Size"], 1132420232)

class testRedisBloomNoCodec():
    def __init__(self):
        self.env = Env(decodeResponses=False)

    def test_scandump(self):
        env = self.env
        env.cmd('FLUSHALL')
        maxrange = 500
        env.cmd('bf.reserve', 'bf', 0.01, int(maxrange / 8))
        for x in range(maxrange):
            env.cmd('bf.add', 'bf', str(x))
        for x in range(maxrange):
            env.assertEqual(1, env.cmd('bf.exists', 'bf', str(x)))
        # Start with scandump
        env.assertRaises(ResponseError, env.cmd, 'bf.scandump', 'bf')
        env.assertRaises(ResponseError, env.cmd, 'bf.scandump', 'bf', 'str')
        env.assertRaises(ResponseError, env.cmd, 'bf.scandump', 'noexist', '0')
        chunks = []
        while True:
            last_pos = chunks[-1][0] if chunks else 0
            chunk = env.cmd('bf.scandump', 'bf', last_pos)
            if not chunk[0]:
                break
            chunks.append(chunk)
            # print("Scaning chunk... (P={}. Len={})".format(chunk[0], len(chunk[1])))
        env.cmd('del', 'bf')
        env.assertRaises(ResponseError, env.cmd, 'bf.loadchunk', 'bf')
        env.assertRaises(ResponseError, env.cmd, 'bf.loadchunk', 'bf', 'str')
        for chunk in chunks:
            print("Loading chunk... (P={}. Len={})".format(chunk[0], len(chunk[1])))
            env.cmd('bf.loadchunk', 'bf', *chunk)
        for x in range(maxrange):
            env.assertEqual(1, env.cmd('bf.exists', 'bf', str(x)))

    def test_scandump_with_expansion(self):
        env = self.env
        env.cmd('FLUSHALL')
        maxrange = 500

        env.cmd('bf.reserve', 'bf', 0.01, int(maxrange / 8), 'expansion', '2')
        for x in range(maxrange):
            env.cmd('bf.add', 'bf', str(x))
        for x in range(maxrange):
            env.assertEqual(1, env.cmd('bf.exists', 'bf', str(x)))

        chunks = []
        while True:
            i = 0
            last_pos = chunks[-1][0] if chunks else 0
            chunk = env.cmd('bf.scandump', 'bf', last_pos)
            if not chunk[0]:
                break
            chunks.append(chunk)
            print("Scaning chunk... (P={}. Len={})".format(chunk[0], len(chunk[1])))
        # delete filter
        env.cmd('del', 'bf')

        env.assertRaises(ResponseError, env.cmd, 'bf.loadchunk', 'bf')
        env.assertRaises(ResponseError, env.cmd, 'bf.loadchunk', 'bf', 'str')
        for chunk in chunks:
            print("Loading chunk... (P={}. Len={})".format(chunk[0], len(chunk[1])))
            env.cmd('bf.loadchunk', 'bf', *chunk)
        # check loaded filter
        for x in range(maxrange):
            env.assertEqual(1, env.cmd('bf.exists', 'bf', str(x)))

    def test_scandump_huge(self):
        env = self.env
        env.cmd('FLUSHALL')

        env.cmd('bf.reserve', 'bf', 0.01, 1024 * 1024 * 64)
        for x in range(6):
            env.cmd('bf.add', 'bf', 'foo')
        for x in range(6):
            env.assertEqual(1, env.cmd('bf.exists', 'bf', 'foo'))

        chunks = []
        while True:
            i = 0
            last_pos = chunks[-1][0] if chunks else 0
            chunk = env.cmd('bf.scandump', 'bf', last_pos)
            if not chunk[0]:
                break
            chunks.append(chunk)
            print("Scaning chunk... (P={}. Len={})".format(chunk[0], len(chunk[1])))
        # delete filter
        env.cmd('del', 'bf')

        env.assertRaises(ResponseError, env.cmd, 'bf.loadchunk', 'bf')
        env.assertRaises(ResponseError, env.cmd, 'bf.loadchunk', 'bf', 'str')
        for chunk in chunks:
            print("Loading chunk... (P={}. Len={})".format(chunk[0], len(chunk[1])))
            env.cmd('bf.loadchunk', 'bf', *chunk)
        # check loaded filter
        for x in range(6):
            env.assertEqual(1, env.cmd('bf.exists', 'bf', 'foo'))
