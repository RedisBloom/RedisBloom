
from common import *
import time


class testTopK():
    def __init__(self):
        self.env = Env(decodeResponses=True)
        self.assertOk = self.env.assertTrue
        self.cmd = self.env.cmd
        self.assertEqual = self.env.assertEqual
        self.assertRaises = self.env.assertRaises
        self.assertTrue = self.env.assertTrue
        self.assertAlmostEqual = self.env.assertAlmostEqual
        self.assertGreater = self.env.assertGreater

    def test_simple(self):
        self.cmd('FLUSHALL')
        self.assertTrue(self.cmd('topk.reserve', 'topk', '20', '50', '5', '0.9'))
        self.assertEqual([None], self.cmd('topk.add', 'topk', 'a'))
        self.assertEqual([None, None], self.cmd('topk.add', 'topk', 'a', 'b'))

        self.assertEqual([1], self.cmd('topk.query', 'topk', 'a'))
        self.assertEqual([0], self.cmd('topk.query', 'topk', 'c'))

    def test_validation(self):
        self.cmd('FLUSHALL')
        for args in (
                (),
                ('foo',),
                ('foo', 'bar'),
                ('foo', 'bar', 'baz'),
                ('foo', 'bar', 'baz', 'Qux'),
                ('foo', 'bar', 'baz', 'Qux', 'Quux', 'xyzzy'),
                ('foo', 'bar', '50', '5', '0.9'),
                ('foo', '20', 'bar', '5', '0.9'),
                ('foo', '20', '50', 'bar', '0.9'),
                ('foo', '20', '50', '5', 'bar'),
                ('foo', '0.9', '50', '5', '0.9'),
                ('foo', '20', '0.9', '5', '0.9'),
                ('foo', '20', '50', '0.9', '0.9'),
                ('foo', '20', '50', '5', '42'),
        ):
            self.assertRaises(ResponseError, self.cmd, 'topk.reserve', *args)

        for args in ((), ('test',)):
            for cmd in ('topk.add', 'topk.query', 'topk.count', 'topk.list' 'topk.info'):
                self.assertRaises(ResponseError, self.cmd, cmd, *args)

        for args in ((), ('topk',)):
            for cmd in ('topk.add', 'topk.query', 'topk.count'):
                self.assertRaises(ResponseError, self.cmd, cmd, *args)

        self.cmd('SET', 'king', 'kong')
        self.assertRaises(ResponseError, self.cmd, 'topk.add', 'king', 'kong')
        self.assertRaises(ResponseError, self.cmd, 'topk.add', 'kong', 'kong')

    def test_add_query_count(self):
        self.cmd('FLUSHALL')
        self.assertTrue(self.cmd('topk.reserve', 'topk', '20', '50', '5', '0.9'))
        yield 1
        self.env.dumpAndReload(restart=True) # prevent error `Background save already in progress`
        yield 2

        self.cmd('topk.add', 'topk', 'bar', 'baz', '42')
        self.assertEqual([1], self.cmd('topk.query', 'topk', 'bar'))
        self.assertEqual([0], self.cmd('topk.query', 'topk', 'foo'))
        self.assertEqual([0, 1, 1], self.cmd('topk.query',
                                             'topk', 'foo', 'bar', 'baz'))
        self.assertEqual([1], self.cmd('topk.count', 'topk', 'bar'))
        self.assertEqual([0], self.cmd('topk.count', 'topk', 'foo'))
        self.assertEqual([0, 1, 1], self.cmd('topk.count',
                                             'topk', 'foo', 'bar', 'baz'))

        self.cmd('topk.add', 'topk', 'bar', 'baz')
        self.assertEqual([0, 2, 2, 1], self.cmd('topk.count',
                                                'topk', 'foo', 'bar', 'baz', '42'))

        self.assertEqual([None], self.cmd('topk.add', 'topk', 'foo'))
        self.assertEqual([1], self.cmd('topk.query', 'topk', 'foo'))
        self.assertEqual([0], self.cmd('topk.query', 'topk', 'xyxxy'))

        self.assertEqual([None, None, None, None], self.cmd('topk.add', 'topk', 'foo', '1', 'bar', '1'))
        # TODO: check if required after RLTest moving
        # self.env.dumpAndReload()
        # self.assertEqual([2], self.cmd('topk.count', 'topk', 'foo'))
        # self.assertEqual([3], self.cmd('topk.count', 'topk', 'bar'))
        # self.assertEqual([0], self.cmd('topk.count', 'topk', 'nonexist'))

    def test_incrby(self):
        self.cmd('FLUSHALL')
        self.assertTrue(self.cmd('topk.reserve', 'topk', '3', '10', '3', '1'))
        self.assertEqual([None, None, None], self.cmd('topk.incrby', 'topk', 'bar', 3, 'baz', 6, '42', 2))
        self.assertEqual([None, 'bar'], self.cmd('topk.incrby', 'topk', '42', 8, 'xyzzy', 4))
        self.assertEqual([3, 6, 10, 4, 0], self.cmd('topk.count', 'topk', 'bar', 'baz', '42', 'xyzzy', 4))
        self.assertRaises(ResponseError, self.cmd, 'topk.incrby')
        self.assertTrue(isinstance(self.cmd('topk.incrby', 'topk', 'foo', -5)[0], ResponseError))
        self.assertTrue(isinstance(self.cmd('topk.incrby', 'topk', 'foo', 123456)[0], ResponseError))

    def test_lookup_table(self):
        self.cmd('FLUSHALL')
        self.assertTrue(self.cmd('topk.reserve', 'topk', '1', '3', '3', '.9'))
        self.cmd('topk.incrby', 'topk', 'bar', 300, 'baz', 600, '42', 200)
        self.cmd('topk.incrby', 'topk', '42', 80, 'xyzzy', 400)
        self.assertEqual(['baz'], self.cmd('topk.list', 'topk'))

    def test_list_info(self):
        self.cmd('FLUSHALL')
        self.cmd('topk.reserve', 'topk', '2', '50', '5', '0.9')
        self.assertRaises(ResponseError, self.cmd, 'topk.reserve', 'topk', '2', '50', '5', '0.9')
        self.cmd('topk.add', 'topk', 'foo', 'bar', 'baz', '42', 'foo', 'bar', 'baz', )
        self.cmd('topk.add', 'topk', 'foo', 'baz', '42', 'foo', 'baz', )
        self.cmd('topk.add', 'topk', 'foo', 'bar', 'baz', 'foo', 'baz', )
        self.assertEqual(['foo', 'baz'], self.cmd('topk.list', 'topk'))
        self.assertEqual([None, None, 'foo', None, None],
                         self.cmd('topk.add', 'topk', 'bar', 'bar', 'bar', 'bar', 'bar'))
        self.assertEqual(['bar', 'baz'], self.cmd('topk.list', 'topk'))

        self.assertRaises(ResponseError, self.cmd, 'topk.list', 'topk', '_topk_')

        info = self.cmd('topk.info', 'topk')
        self.assertEqual('k', info[0])
        self.assertEqual(2, info[1])
        self.assertEqual('width', info[2])
        self.assertEqual(50, info[3])
        self.assertEqual('depth', info[4])
        self.assertEqual(5, info[5])
        self.assertEqual('decay', info[6])
        self.assertAlmostEqual(0.9, float(info[7]), 0.1)
        self.assertRaises(ResponseError, self.cmd, 'topk.info', 'topk', '_topk_')

        # TODO: check if required after RLTest moving
        # self.env.dumpAndReload()
        # self.cmd('topk.reserve', 'test', '3', '50', '5', '0.9')
        # self.cmd('topk.add', 'test', 'foo')
        # self.assertEqual([None, 'foo', None], self.cmd('topk.list', 'test'))
        # self.assertEqual(4192, self.cmd('MEMORY USAGE', 'test'))

    def test_time(self):
        self.cmd('FLUSHALL')
        self.cmd('topk.reserve', 'topk', '100', '1000', '5', '0.9')

        for _ in range(10):
            for i in range(50):
                self.cmd('topk.add', 'topk', i * 100)

        for _ in range(5):
            for i in range(5000):
                self.cmd('topk.add', 'topk', i)

        for _ in range(5):
            for i in range(100):
                self.cmd('topk.add', 'topk', i * 50)

        heapList = self.cmd('topk.list', 'topk')
        self.assertEqual(100, len(heapList))
        res = sum(1 for i in range(len(heapList)) if (int(heapList[i]) % 100 == 0))
        self.assertGreater(res, 45)

    def test_no_init_params(self):
        self.cmd('FLUSHALL')
        self.cmd('topk.reserve', 'topk', '3')
        self.cmd('topk.add', 'topk', 'foo', 'bar', 'baz', '42', 'foo', 'bar', 'baz', )
        self.cmd('topk.add', 'topk', 'foo', 'baz', '42', 'foo', 'baz', )
        self.cmd('topk.add', 'topk', 'foo', 'bar', 'baz', 'foo', 'baz', )
        heapList = self.cmd('topk.list', 'topk')
        self.assertEqual(['foo', 'baz', 'bar'], heapList)

        info = self.cmd('topk.info', 'topk')
        expected_info = ['k', 3, 'width', 8, 'depth', 7, 'decay']
        expected_decay = float('0.90000000000000002')
        self.assertEqual(expected_info, info[:-1])
        self.assertEqual(expected_decay, float(info[-1:][0]))

    def test_list_with_count(self):
        self.cmd('FLUSHALL')
        self.cmd('topk.reserve', 'topk', '3')
        self.cmd('topk.add', 'topk', 'foo', 'bar', 'baz', '42', 'foo', 'bar', 'baz', )
        self.cmd('topk.add', 'topk', 'foo', 'baz', '42', 'foo', )
        self.cmd('topk.add', 'topk', 'foo', 'bar', 'baz', 'foo', )
        heapList = self.cmd('topk.list', 'topk', 'WITHCOUNT')
        self.assertEqual(['foo', 6, 'baz', 4, 'bar', 3], heapList)

    def test_list_no_duplicates(self):
        self.cmd('FLUSHALL')
        self.cmd('topk.reserve', 'topk', '10', '8', '7', '1')
        self.cmd('topk.add', 'topk', 'j', 'h', 'd', 'j', 'h', 'h', 'j', 'g', 'e', 'g', 'i', 'f', 'g', 'f', 'a', 'j', 'c', 'i', 'a', 'd')
        heapList = self.cmd('topk.list', 'topk')
        self.assertEqual(len(set(heapList)), len(heapList))
