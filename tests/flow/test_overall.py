#!/usr/bin/env python3
from RLTest import Env
from redis import ResponseError

xrange = range


def ConvertInfo(lst):
    res_dct = {lst[i]: lst[i + 1] for i in range(0, len(lst), 2)}
    return res_dct


class testRedisBloom():
    def __init__(self):
        self.env = Env(decodeResponses=True)
        self.assertOk = self.env.assertTrue
        self.cmd = self.env.cmd
        self.assertEqual = self.env.assertEqual
        self.assertRaises = self.env.assertRaises
        self.assertTrue = self.env.assertTrue
        self.assertAlmostEqual = self.env.assertAlmostEqual
        self.assertGreater = self.env.assertGreater
        self.restart_and_reload = self.env.restartAndReload
        self.assertResponseError = self.env.assertResponseError
        self.retry_with_rdb_reload = self.env.dumpAndReload
        self.assertNotEqual = self.env.assertNotEqual
        self.assertGreaterEqual = self.env.assertGreaterEqual
        self.assertLessEqual = self.env.assertLessEqual
        self.assertLess = self.env.assertLess

    def test_custom_filter(self):
        self.cmd('FLUSHALL')
        self.assertOk(self.cmd('bf.reserve', 'test', '0.05', '1000'))
        self.assertEqual([1, 1, 1], self.cmd(
            'bf.madd', 'test', 'foo', 'bar', 'baz'))
        self.retry_with_rdb_reload()

        self.assertEqual(1, self.cmd('bf.exists', 'test', 'foo'))
        self.assertEqual(0, self.cmd('bf.exists', 'test', 'nonexist'))

        with self.assertResponseError():
            self.cmd('bf.reserve', 'test', '0.01')

        # Ensure there's no error when trying to add elements to it:
        self.retry_with_rdb_reload()
        self.assertEqual(0, self.cmd('bf.add', 'test', 'foo'))

    def test_set(self):
        self.cmd('FLUSHALL')
        self.assertEqual(1, self.cmd('bf.add', 'test', 'foo'))
        self.assertEqual(1, self.cmd('bf.exists', 'test', 'foo'))
        self.assertEqual(0, self.cmd('bf.exists', 'test', 'bar'))

        # Set multiple keys at once:
        self.assertEqual([0, 1], self.cmd('bf.madd', 'test', 'foo', 'bar'))

        self.retry_with_rdb_reload()
        self.assertEqual(1, self.cmd('bf.exists', 'test', 'foo'))
        self.assertEqual(1, self.cmd('bf.exists', 'test', 'bar'))
        self.assertEqual(0, self.cmd('bf.exists', 'test', 'nonexist'))

    def test_multi(self):
        self.cmd('FLUSHALL')
        self.assertEqual([1, 1, 1], self.cmd(
            'bf.madd', 'test', 'foo', 'bar', 'baz'))
        self.assertEqual([0, 1, 0], self.cmd(
            'bf.madd', 'test', 'foo', 'new1', 'bar'))

        self.assertEqual([0], self.cmd('bf.mexists', 'test', 'nonexist'))

        self.assertEqual([0, 1], self.cmd(
            'bf.mexists', 'test', 'nonexist', 'foo'))

    def test_validation(self):
        self.cmd('FLUSHALL')
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
            self.assertRaises(ResponseError, self.cmd, 'bf.reserve', *args)

        self.assertOk(self.cmd('bf.reserve', 'test', '0.01', '1000'))
        for args in ((), ('test',)):
            for cmd in ('bf.add', 'bf.madd', 'bf.exists', 'bf.mexists'):
                self.assertRaises(ResponseError, self.cmd, cmd, *args)

        for cmd in ('bf.exists', 'bf.add'):
            self.assertRaises(ResponseError, self.cmd, cmd, 'test', 1, 2)

        self.cmd('set', 'foo', 'bar')
        self.assertRaises(ResponseError, self.cmd, 'bf.reserve', 'foo')

    def test_oom(self):
        self.cmd('FLUSHALL')
        self.assertRaises(ResponseError, self.cmd, 'bf.reserve', 'test', 0.01, 4294967296 * 4294967296)

    def test_rdb_reload(self):
        self.cmd('FLUSHALL')
        self.assertEqual(1, self.cmd('bf.add', 'test', 'foo'))
        yield 1
        self.env.dumpAndReload()
        yield 2
        self.assertEqual(1, self.cmd('bf.exists', 'test', 'foo'))
        self.assertEqual(0, self.cmd('bf.exists', 'test', 'bar'))

    def test_dump_and_load(self):
        self.cmd('FLUSHALL')
        # Store a filter
        quantity = 1000
        error_rate = 0.001
        self.cmd('bf.reserve', 'myBloom', error_rate, quantity * 1024 * 8)

        # test is probabilistic and might fail. It is OK to change variables if
        # certain to not break anything
        def do_verify(add):
            false_positives = 0.0
            for x in xrange(quantity):
                if add:
                    self.cmd('bf.add', 'myBloom', x)
                rv = self.cmd('bf.exists', 'myBloom', x)
                self.assertTrue(rv)
                rv = self.cmd('bf.exists', 'myBloom', 'nonexist_{}'.format(x))
                if rv == 1:
                    false_positives += 1
            self.assertLessEqual(false_positives / quantity, error_rate)

        with self.assertResponseError():
            self.cmd('bf.scandump', 'myBloom')
        with self.assertResponseError():
            self.cmd('bf.scandump', 'myBloom', 'str')
        with self.assertResponseError():
            self.cmd('bf.scandump', 'myCuckoo', 0)
        with self.assertResponseError():
            self.cmd('bf.loadchunk', 'myBloom')
        with self.assertResponseError():
            self.cmd('bf.loadchunk', 'myBloom', 'str', 'data')

            # TODO: Enable this portion again after RLTest migration
        # do_verify(add=True)
        # cmds = []
        # cur = self.cmd('bf.scandump', 'myBloom', 0)
        # first = cur[0]
        # cmds.append(cur)

        # while True:
        #     cur = self.cmd('bf.scandump', 'myBloom', first)
        #     first = cur[0]
        #     if first == 0:
        #         break
        #     else:
        #         cmds.append(cur)
        #         print("Scaning chunk... (P={}. Len={})".format(cur[0], len(cur[1])))

        # prev_info = self.cmd('bf.debug', 'myBloom')
        # # Remove the filter
        # self.cmd('del', 'myBloom')

        # # Now, load all the commands:
        # for cmd in cmds:
        #     print("Loading chunk... (P={}. Len={})".format(cmd[0], len(cmd[1])))
        #     self.cmd('bf.loadchunk', 'myBloom', *cmd)

        # cur_info = self.cmd('bf.debug', 'myBloom')
        # self.assertEqual(prev_info, cur_info)
        # do_verify(add=False)

        # # Try a bigger one
        # self.cmd('del', 'myBloom')
        # self.cmd('bf.reserve', 'myBloom', '0.0001', '10000000')

    def test_missing(self):
        self.cmd('FLUSHALL')
        res = self.cmd('bf.exists', 'myBloom', 'foo')
        self.assertEqual(0, res)
        res = self.cmd('bf.mexists', 'myBloom', 'foo', 'bar', 'baz')
        self.assertEqual([0, 0, 0], res)

    def test_insert(self):
        self.cmd('FLUSHALL')
        with self.assertResponseError():
            self.cmd('bf.insert', 'missingFilter', 'NOCREATE', 'ITEMS', 'foo', 'bar')
        with self.assertResponseError():
            self.cmd('bf.insert', 'missingFilter', 'DONTEXIST', 'NOCREATE')
        with self.assertResponseError():
            self.cmd('bf.insert', 'missingFilter')
        with self.assertResponseError():
            self.cmd('bf.insert', 'missingFilter', 'NOCREATE', 'ITEMS')
        with self.assertResponseError():
            self.cmd('bf.insert', 'missingFilter', 'NOCREATE', 'CAPACITY')
        with self.assertResponseError():
            self.cmd('bf.insert', 'missingFilter', 'CAPACITY', 0.3)
        with self.assertResponseError():
            self.cmd('bf.insert', 'missingFilter', 'ERROR')
        with self.assertResponseError():
            self.cmd('bf.insert', 'missingFilter', 'ERROR', 'big')
        with self.assertResponseError():
            self.cmd('bf.insert', 'missingFilter', 'EXPANSION', '0', 'ITEMS', 'foo')
        with self.assertResponseError():
            self.cmd('bf.insert', 'missingFilter', 'EXPANSION', 'big')

        rep = self.cmd('BF.INSERT', 'missingFilter', 'ERROR',
                       '0.001', 'CAPACITY', '50000', 'EXPANSION', 2, 'ITEMS', 'foo')
        self.assertEqual([1], rep)
        self.assertEqual(
            ['size:1', 'bytes:98880 bits:791040 hashes:11 hashwidth:64 capacity:50000 size:1 ratio:0.0005'],
            [x for x in self.cmd('bf.debug', 'missingFilter')])

        rep = self.cmd('BF.INSERT', 'missingFilter', 'ERROR', '0.1', 'ITEMS', 'foo', 'bar', 'baz')
        self.assertEqual([0, 1, 1], rep)
        self.assertEqual(
            ['size:3', 'bytes:98880 bits:791040 hashes:11 hashwidth:64 capacity:50000 size:3 ratio:0.0005'],
            [x for x in self.cmd('bf.debug', 'missingFilter')])

    def test_mem_usage(self):
        self.cmd('FLUSHALL')
        self.assertOk(self.cmd('bf.reserve', 'bf', '0.05', '1000'))
        if self.env.isDebugger() is False:
            self.assertEqual(1088, self.cmd('MEMORY USAGE', 'bf'))
        self.assertEqual([1, 1, 1], self.cmd(
            'bf.madd', 'bf', 'foo', 'bar', 'baz'))
        if self.env.isDebugger() is False:
            self.assertEqual(1088, self.cmd('MEMORY USAGE', 'bf'))
        with self.assertResponseError():
            self.cmd('bf.debug', 'bf', 'noexist')
        with self.assertResponseError():
            self.cmd('bf.debug', 'cf')

    def test_expansion(self):
        self.cmd('FLUSHALL')
        self.assertOk(self.cmd('bf.reserve exp1 0.01 4 expansion 1'))
        self.assertOk(self.cmd('bf.reserve exp2 0.01 4 expansion 2'))
        self.assertOk(self.cmd('bf.reserve exp4 0.01 4 expansion 4'))
        for i in range(100):
            self.cmd('bf.add exp1', str(i))
            self.cmd('bf.add exp2', str(i))
            self.cmd('bf.add exp4', str(i))
        for i in range(100):
            self.assertEqual(1, self.cmd('bf.exists exp1', str(i)))
            self.assertEqual(1, self.cmd('bf.exists exp2', str(i)))
            self.assertEqual(1, self.cmd('bf.exists exp4', str(i)))

        self.assertEqual(23, self.cmd('bf.info', 'exp1')[5])
        self.assertEqual(5, self.cmd('bf.info', 'exp2')[5])
        self.assertEqual(4, self.cmd('bf.info', 'exp4')[5])

        with self.assertResponseError():
            self.cmd('bf.reserve exp4 0.01 4 expansion')
        with self.assertResponseError():
            self.cmd('bf.reserve exp4 0.01 4 expansion 0')
        with self.assertResponseError():
            self.cmd('bf.reserve exp4 0.01 4 expansion str')

    def test_debug(self):
        self.cmd('FLUSHALL')
        self.assertOk(self.cmd('bf.reserve', 'bf', '0.01', '10'))
        for i in range(100):
            self.cmd('bf.add', 'bf', str(i))
        self.assertEqual(self.cmd('bf.debug', 'bf'), ['size:99',
                                                      'bytes:16 bits:128 hashes:8 hashwidth:64 capacity:10 size:10 ratio:0.005',
                                                      'bytes:32 bits:256 hashes:9 hashwidth:64 capacity:20 size:20 ratio:0.0025',
                                                      'bytes:72 bits:576 hashes:10 hashwidth:64 capacity:40 size:40 ratio:0.00125',
                                                      'bytes:160 bits:1280 hashes:11 hashwidth:64 capacity:80 size:29 ratio:0.000625'])

        self.cmd('del', 'bf')

        self.assertOk(self.cmd('bf.reserve', 'bf', '0.001', '100'))
        for i in range(4000):
            self.cmd('bf.add', 'bf', str(i))
        self.assertEqual(self.cmd('bf.debug', 'bf'), ['size:3997',
                                                      'bytes:200 bits:1600 hashes:11 hashwidth:64 capacity:100 size:100 ratio:0.0005',
                                                      'bytes:432 bits:3456 hashes:12 hashwidth:64 capacity:200 size:200 ratio:0.00025',
                                                      'bytes:936 bits:7488 hashes:13 hashwidth:64 capacity:400 size:400 ratio:0.000125',
                                                      'bytes:2016 bits:16128 hashes:14 hashwidth:64 capacity:800 size:800 ratio:6.25e-05',
                                                      'bytes:4320 bits:34560 hashes:15 hashwidth:64 capacity:1600 size:1600 ratio:3.125e-05',
                                                      'bytes:9216 bits:73728 hashes:16 hashwidth:64 capacity:3200 size:897 ratio:1.5625e-05'])

    def test_info(self):
        self.cmd('FLUSHALL')
        self.assertOk(self.cmd('bf.reserve', 'bf', '0.001', '100'))
        self.assertEqual(self.cmd('bf.info bf'), ['Capacity', 100,
                                                  'Size', 352,
                                                  'Number of filters', 1,
                                                  'Number of items inserted', 0,
                                                  'Expansion rate', 2])

        with self.assertResponseError():
            self.cmd('bf.info', 'cf')
        with self.assertResponseError():
            self.cmd('bf.info')

    def test_no_1_error_rate(self):
        self.cmd('FLUSHALL')
        with self.assertResponseError():
            self.cmd('bf.reserve bf 1 1000')

    def test_error_rate(self):
        self.cmd('FLUSHALL')
        repeat = 1024
        rates = [0.1, 0.01, 0.001, 0.0001]
        names = ['bf0.1', 'bf0.01', 'bf0.001', 'bf0.0001']

        for i in range(len(rates)):
            false_positive = 0.0
            self.cmd('bf.reserve', names[i], rates[i], repeat)
            for x in range(repeat):
                self.cmd('bf.add', names[i], x)
            for x in range(repeat, repeat * 11):
                false_positive += self.cmd('bf.exists', names[i], x)
            self.assertGreaterEqual(rates[i], false_positive / (repeat * 10))

    def test_no_scaling(self):
        self.cmd('FLUSHALL')
        capacity = 3
        self.assertOk(self.cmd('bf.reserve bf 0.01', capacity, 'nonscaling'))
        for i in range(capacity):
            self.assertEqual(1, self.cmd('bf.add bf', i))
        with self.assertResponseError():
            self.cmd('bf.add bf extra')
        with self.assertResponseError():
            self.cmd('bf.reserve bf_mix 0.01 1000 nonscaling expansion 2')

        self.assertOk(self.cmd('bf.reserve bfnonscale 0.001 1000 nonscaling'))
        self.assertOk(self.cmd('bf.reserve bfscale 0.001 1000'))
        self.assertLess(self.cmd('bf.info bfnonscale')[3], self.cmd('bf.info bfscale')[3])

    def test_nonscaling_err(self):
        self.cmd('FLUSHALL')
        capacity = 3
        self.assertEqual([1, 1, 1], self.cmd('BF.INSERT nonscaling_err CAPACITY 3 NONSCALING ITEMS a b c'))
        resp = self.cmd('BF.INSERT nonscaling_err ITEMS a b c d d')
        self.assertEqual([0, 0, 0, ], resp[:3])
        self.assertEqual('non scaling filter is full', str(resp[3]))
        info_actual = self.cmd('BF.INFO nonscaling_err')
        info_expected = ['Capacity', 3, 'Size', 160, 'Number of filters', 1,
                         'Number of items inserted', 3, 'Expansion rate', None]
        self.assertEqual(info_actual, info_expected)

    def test_issue178(self):
        self.cmd('FLUSHALL')
        capacity = 300 * 1000 * 1000
        error_rate = 0.000001
        self.assertOk(self.cmd('bf.reserve bf', error_rate, capacity))
        info = ConvertInfo(self.cmd('bf.info bf'))
        self.assertEqual(info["Capacity"], 300000000)
        self.assertEqual(info["Size"], 1132420288)
