
from common import *
from random import randint
import redis


class testCMS():
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
        self.assertOk(self.cmd('cms.initbydim', 'cms1', '20', '5'))
        self.assertEqual([5], self.cmd('cms.incrby', 'cms1', 'a', '5'))
        self.assertEqual([5], self.cmd('cms.query', 'cms1', 'a'))
        self.assertEqual(['width', 20, 'depth', 5, 'count', 5],
                         self.cmd('cms.info', 'cms1'))

        self.assertOk(self.cmd('cms.initbyprob', 'cms2', '0.001', '0.01'))
        self.assertEqual([5], self.cmd('cms.incrby', 'cms2', 'a', '5'))
        self.assertEqual([5], self.cmd('cms.query', 'cms2', 'a'))
        self.assertEqual(['width', 2000, 'depth', 7, 'count', 5],
                         self.cmd('cms.info', 'cms2'))
        yield 1
        self.env.dumpAndReload()
        yield 2
        if not VALGRIND:
            if server_version_at_least(self.env, '7.0.0'):
                self.assertEqual(856, self.cmd('MEMORY USAGE', 'cms1'))
            else:
                self.assertEqual(840, self.cmd('MEMORY USAGE', 'cms1'))

    def test_validation(self):
        self.cmd('FLUSHALL')
        for args in (
                (),
                ('foo',),
                ('foo', '0.1'),
                ('foo', '0.1', 'blah'),
                ('foo', '10'),
                ('foo', '10', 'blah'),
                ('foo', 'blah', '10'),
                ('foo', '0', '0'),
                ('foo', '0', '100'),
                ('foo', '100', '0'),
                ('foo', '8589934592', '8589934592'),
                ('foo', '4611686018427388100', '1'),
                ('foo', '2', '2611686018427388100'),
        ):
            self.assertRaises(ResponseError, self.cmd, 'cms.initbydim', *args)

        for args in (
                (),
                ('foo',),
                ('foo', '1000'),
                ('foo', '0.1'),
                ('foo', '1000', '0.1'),
                ('foo', '1000', 'blah'),
                ('foo', '1000', '10'),
                ('foo', '0.1', 'blah'),
                ('foo', '10', 'blah'),
                ('foo', 'blah', '10'),
                ('foo', '0', '0'),
                ('foo', '1000', '0',),
                ('foo', '0', '100'),
                ('foo', '0.9', '0.9999999999999999'),
                ('foo', '0.0000000000000000001', '0.9'),
        ):
            self.assertRaises(ResponseError, self.cmd, 'cms.initbyprob', *args)

        self.assertRaises(ResponseError, self.cmd, 'cms.initbydim', '0.1', '0.1')
        self.assertRaises(ResponseError, self.cmd, 'cms.initbyprob', '10', '10')

        self.assertOk(self.cmd('cms.initbydim', 'testDim', '100', '5'))
        self.assertOk(self.cmd('cms.initbyprob', 'testProb', '0.1', '0.1'))

        for args in ((), ('test',)):
            for cmd in ('cms.incrby', 'cms.query', 'cms.merge', 'cms.info'):
                self.assertRaises(ResponseError, self.cmd, cmd, *args)

    def test_incrby_query(self):
        self.cmd('FLUSHALL')
        self.cmd('SET', 'A', 'B')
        self.cmd('cms.initbydim', 'cms', '1000', '5')
        self.cmd('cms.incrby', 'cms', 'bar', '5', 'baz', '42')
        self.assertEqual([0], self.cmd('cms.query', 'cms', 'foo'))
        self.assertEqual([0, 5, 42], self.cmd('cms.query',
                                              'cms', 'foo', 'bar', 'baz'))
        self.assertRaises(ResponseError, self.cmd, 'cms.incrby', 'noexist', 'bar', '5')
        self.assertRaises(ResponseError, self.cmd, 'cms.incrby', 'A', 'bar', '5')
        self.assertRaises(ResponseError, self.cmd, 'cms.incrby',
                          'cms', 'bar', '5', 'baz')
        self.assertRaises(ResponseError, self.cmd, 'cms.incrby',
                          'cms', 'bar', '5', 'baz')
        self.assertEqual([0, 5, 42], self.cmd('cms.query',
                                              'cms', 'foo', 'bar', 'baz'))

        # c = self.client
        self.cmd('cms.initbydim', 'test', '1000', '5')
        self.assertEqual([1], self.cmd('cms.incrby', 'test', 'foo', '1'))
        self.assertEqual([1], self.cmd('cms.query', 'test', 'foo'))
        self.assertEqual([0], self.cmd('cms.query', 'test', 'bar'))

        self.assertEqual([2, 1], self.cmd('cms.incrby', 'test', 'foo', '1', 'bar', '1'))
        # for _ in c.retry_with_rdb_reload():
        #     self.assertEqual([2], self.cmd('cms.query', 'test', 'foo'))
        #     self.assertEqual([1], self.cmd('cms.query', 'test', 'bar'))
        #     self.assertEqual([0], self.cmd('cms.query', 'test', 'nonexist'))

    def test_merge(self):
        self.cmd('FLUSHALL')
        self.cmd('cms.initbydim', 'small_1', '20', '5')
        self.cmd('cms.initbydim', 'small_2', '20', '5')
        self.cmd('cms.initbydim', 'small_3', '20', '5')
        self.cmd('cms.initbydim', 'large_4', '2000', '10')
        self.cmd('cms.initbydim', 'large_5', '2000', '10')
        self.cmd('cms.initbydim', 'large_6', '2000', '10')

        # empty small batch
        self.assertOk(self.cmd('cms.merge', 'small_3', 2, 'small_1', 'small_2'))
        self.assertEqual(['width', 20, 'depth', 5, 'count', 0],
                         self.cmd('cms.info', 'small_3'))

        # empty large batch
        self.assertOk(self.cmd('cms.merge', 'large_6', 2, 'large_4', 'large_5'))
        self.assertEqual(['width', 2000, 'depth', 10, 'count', 0],
                         self.cmd('cms.info', 'large_6'))

        # non-empty small batch
        self.cmd('cms.incrby', 'small_1', 'a', '21')
        self.cmd('cms.incrby', 'small_2', 'a', '21')
        self.assertOk(self.cmd('cms.merge', 'small_3', 2, 'small_1', 'small_2'))
        self.assertEqual([42], self.cmd('cms.query', 'small_3', 'a'))

        # non-empty small batch
        self.cmd('cms.incrby', 'large_4', 'a', '21')
        self.cmd('cms.incrby', 'large_5', 'a', '21')
        self.assertOk(self.cmd('cms.merge', 'large_6', 2, 'large_4', 'large_5'))
        self.assertEqual([42], self.cmd('cms.query', 'large_6', 'a'))

        # mixed batch
        self.assertRaises(ResponseError, self.cmd, 'cms.merge', 'small_3', 2,
                          'small_2', 'large_5')

    def test_errors(self):
        self.cmd('FLUSHALL')
        self.cmd('SET', 'A', '2000')
        self.assertRaises(ResponseError, self.cmd, 'cms.initbydim', 'A', '2000', '10')
        self.assertRaises(ResponseError, self.cmd, 'cms.incrby', 'A', 'foo')
        self.assertRaises(ResponseError, self.cmd, 'cms.incrby', 'B', '5')
        self.assertRaises(ResponseError, self.cmd, 'cms.info', 'A')

        self.assertOk(self.cmd('cms.initbydim', 'foo', '2000', '10'))
        self.assertOk(self.cmd('cms.initbydim', 'bar', '2000', '10'))
        self.assertOk(self.cmd('cms.initbydim', 'baz', '2000', '10'))
        self.assertRaises(ResponseError, self.cmd, 'cms.incrby', 'foo', 'item', 'foo')
        self.assertRaises(ResponseError, self.cmd, 'cms.incrby', 'foo', 'item', '-1')
        self.assertRaises(ResponseError, self.cmd, 'cms.merge', 'foo', 2, 'foo')
        self.assertRaises(ResponseError, self.cmd, 'cms.merge', 'foo', 'B', 3, 'foo')
        self.assertRaises(ResponseError, self.cmd, 'cms.merge', 'foo', 1, 'bar', 'weights', 'B')
        self.assertRaises(ResponseError, self.cmd, 'cms.merge', 'foo', 3, 'foo', 'weights', 'B')
        self.assertRaises(ResponseError, self.cmd, 'cms.merge', 'foo', 'A', 'foo', 'weights', 1)
        self.assertRaises(ResponseError, self.cmd, 'cms.merge', 'foo', 3, 'bar', 'baz' 'weights', 1, 'a')
        self.assertRaises(ResponseError, self.cmd, 'cms.merge', 'foo', 3, 'bar', 'baz' 'weights', 1)
        self.assertRaises(ResponseError, self.cmd, 'cms.merge', 'foo', '0')
        self.assertRaises(ResponseError, self.cmd, 'cms.merge', 'foo', '0', 'weights')
        self.assertRaises(ResponseError, self.cmd, 'cms.merge', 'foo', '-1')
        self.assertRaises(ResponseError, self.cmd, 'cms.merge', 'foo', '-1', 'foo', 'bar')
        self.assertRaises(ResponseError, self.cmd, 'cms.merge', 'foo', '-1', 'foo', 'bar', 'weights', 1, 1)

    def test_merge_extensive(self):
        self.cmd('FLUSHALL')
        self.cmd('cms.initbydim', 'A', '2000', '10')
        self.cmd('cms.initbydim', 'B', '2000', '10')
        self.cmd('cms.initbydim', 'C', '2000', '10')

        itemsA = []
        itemsB = []
        for i in range(10000):
            itemsA.append(randint(0, 100))
            self.cmd('cms.incrby', 'A', str(i), itemsA[i])
            itemsB.append(randint(0, 100))
            self.cmd('cms.incrby', 'B', str(i), itemsB[i])
        self.assertOk(self.cmd('cms.merge', 'C', 2, 'A', 'B'))

    def test_overflow(self):
        large_val = 1024*1024*1024*2 - 1

        self.cmd('FLUSHALL')
        self.cmd('cms.initbydim', 'cms', '5', '2')
        self.assertEqual([large_val, 10, 17, 5], self.cmd('cms.incrby', 'cms', 'a', large_val, 'b', 10, 'c', 7, 'd', 5))
        self.assertEqual([large_val, 17, 17, 5], self.cmd('cms.query', 'cms', 'a', 'b', 'c', 'd'))
        self.assertEqual([large_val * 2, 27, 34, 10], self.cmd('cms.incrby', 'cms', 'a', large_val, 'b', 10, 'c', 7, 'd', 5))
        self.assertEqual([large_val * 2, 34, 34, 10], self.cmd('cms.query', 'cms', 'a', 'b', 'c', 'd'))

        # overflow as result > UNIT32_MAX
        res = self.cmd('cms.incrby', 'cms', 'a', large_val, 'b', 10, 'c', 7, 'd', 5)
        # result of insert is an error message
        self.env.assertResponseError(res[0], contained='CMS: INCRBY overflow')
        self.assertEqual(res[1:], [44, 51, 15])
        # result of query in UINT32_MAX (large_val * 2 + 1)
        self.assertEqual([large_val * 2 + 1, 51, 51, 15], self.cmd('cms.query', 'cms', 'a', 'b', 'c', 'd'))

    def test_smallset(self):
        self.cmd('FLUSHALL')
        self.assertOk(self.cmd('cms.initbydim', 'cms1', '2', '2'))
        self.assertEqual([10, 42], self.cmd('cms.incrby', 'cms1', 'foo', '10', 'bar', '42'))
        self.assertEqual([10, 42], self.cmd('cms.query', 'cms1', 'foo', 'bar'))
        self.assertEqual(['width', 2, 'depth', 2, 'count', 52],
                         self.cmd('cms.info', 'cms1'))
        self.assertEqual([10, 42], self.cmd('cms.incrby', 'cms1', 'foo', '0', 'bar', '0'))

    def test_merge_success(self):
        # Merge three sketches and then delete one sketch (merge with -1 weight)
        # Validate content after merge operations.
        self.cmd('FLUSHALL')
        self.assertOk(self.cmd('cms.initbydim', 'cms1{t}', '1000', '5'))
        self.assertOk(self.cmd('cms.initbydim', 'cms2{t}', '1000', '5'))
        self.assertOk(self.cmd('cms.initbydim', 'cms3{t}', '1000', '5'))
        self.assertOk(self.cmd('cms.initbydim', 'cms4{t}', '1000', '5'))
        self.assertOk(self.cmd('cms.initbydim', 'cms5{t}', '1000', '5'))
        self.assertOk(self.cmd('cms.initbydim', 'cms6{t}', '1000', '5'))

        for i in range(0, 100):
            self.assertOk(self.cmd('cms.incrby', 'cms1{t}', 'foo' + str(i), 1))
            self.assertOk(self.cmd('cms.incrby', 'cms2{t}', 'bar' + str(i), 1))
            self.assertOk(self.cmd('cms.incrby', 'cms3{t}', 'baz' + str(i), 1))

        # Merge cms1{t} and cms2{t} into cms4{t}
        self.env.expect('cms.merge', 'cms4{t}', 2, 'cms1{t}', 'cms2{t}', 'weights', '1', '1').ok()
        for i in range(0, 100):
            self.assertEqual([1], self.cmd('cms.query', 'cms4{t}', 'foo' + str(i)))
            self.assertEqual([1], self.cmd('cms.query', 'cms4{t}', 'bar' + str(i)))

        # Merge cms1{t}, cms2{t} and cms3{t} into cms5{t}
        self.env.expect('cms.merge', 'cms5{t}', 3,
                        'cms1{t}', 'cms2{t}', 'cms3{t}',
                        'weights', '1', '1', '1').ok()
        for i in range(0, 100):
            self.assertEqual([1], self.cmd('cms.query', 'cms5{t}', 'foo' + str(i)))
            self.assertEqual([1], self.cmd('cms.query', 'cms5{t}', 'bar' + str(i)))
            self.assertEqual([1], self.cmd('cms.query', 'cms5{t}', 'baz' + str(i)))

        # Delete cms3{t} from cms5{t} and store in cms6{t}
        self.env.expect('cms.merge', 'cms6{t}', 2, 'cms5{t}', 'cms3{t}', 'weights', '1', '-1').ok()
        self.assertEqual(['width', 1000, 'depth', 5, 'count', 200], self.cmd('cms.info', 'cms6{t}'))
        # Validate cms6{t} has cms1{t} and cms2{t} only.
        for i in range(0, 100):
            self.assertEqual([1], self.cmd('cms.query', 'cms6{t}', 'foo' + str(i)))
            self.assertEqual([1], self.cmd('cms.query', 'cms6{t}', 'bar' + str(i)))
            self.assertEqual([0], self.cmd('cms.query', 'cms6{t}', 'baz' + str(i)))

        # Same test as above, negative weight first.
        self.env.expect('cms.merge', 'cms6{t}', 2, 'cms3{t}', 'cms5{t}', 'weights', '-1', '1').ok()
        self.assertEqual(['width', 1000, 'depth', 5, 'count', 200], self.cmd('cms.info', 'cms6{t}'))
        # Validate cms6{t} has cms1{t} and cms2{t} only.
        for i in range(0, 100):
            self.assertEqual([1], self.cmd('cms.query', 'cms6{t}', 'foo' + str(i)))
            self.assertEqual([1], self.cmd('cms.query', 'cms6{t}', 'bar' + str(i)))
            self.assertEqual([0], self.cmd('cms.query', 'cms6{t}', 'baz' + str(i)))

        # Validate you can't delete cms3{t} again.
        self.env.expect('cms.merge', 'cms6{t}', 2, 'cms6{t}', 'cms3{t}',
                        'weights', '1', '-1').error().contains('CMS: MERGE overflow')

        self.env.expect('cms.merge', 'cms6{t}', 2, 'cms3{t}', 'cms6{t}',
                        'weights', '-1', '1').error().contains('CMS: MERGE overflow')

    def test_merge_success_large(self):
        # Create relatively big sketches and verify merge operation works fine
        self.cmd('FLUSHALL')
        self.assertOk(self.cmd('cms.initbydim', 'cms1{t}', '3000', '30'))
        self.assertOk(self.cmd('cms.initbydim', 'cms2{t}', '3000', '30'))
        self.assertOk(self.cmd('cms.initbydim', 'cms3{t}', '3000', '30'))

        for i in range(0, 1000):
            self.assertOk(self.cmd('cms.incrby', 'cms1{t}', 'foo' + str(i), 1))
            self.assertOk(self.cmd('cms.incrby', 'cms2{t}', 'foo' + str(i), 1))
            self.assertOk(self.cmd('cms.incrby', 'cms3{t}', 'bar' + str(i), 1))

        self.env.expect('cms.merge', 'cms1{t}', 2, 'cms1{t}', 'cms2{t}',
                        'weights', '1', '-1').ok()
        self.assertEqual(['width', 3000, 'depth', 30, 'count', 0],
                         self.cmd('cms.info', 'cms1{t}'))

        for i in range(0, 1000):
            self.assertEqual([0], self.cmd('cms.query', 'cms1{t}', 'foo' + str(i)))

        self.env.expect('cms.merge', 'cms1{t}', 2, 'cms1{t}', 'cms2{t}',
                        'weights', '1', '2').ok()
        self.assertEqual(['width', 3000, 'depth', 30, 'count', 2000],
                         self.cmd('cms.info', 'cms1{t}'))

        for i in range(0, 1000):
            self.assertEqual([2], self.cmd('cms.query', 'cms1{t}', 'foo' + str(i)))

        self.env.expect('cms.merge', 'cms1{t}', 2, 'cms1{t}', 'cms2{t}',
                        'weights', '1', '-1').ok()
        self.assertEqual(['width', 3000, 'depth', 30, 'count', 1000],
                         self.cmd('cms.info', 'cms1{t}'))
        for i in range(0, 1000):
            self.assertEqual([1], self.cmd('cms.query', 'cms1{t}', 'foo' + str(i)))

        # Repeatedly add cms3{t} to cms1{t} and verify content.
        for i in range(0, 10):
            self.env.expect('cms.merge', 'cms1{t}', 2, 'cms1{t}', 'cms3{t}',
                            'weights', '1', '1').ok()
            for j in range(0, 1000):
                self.assertEqual([i + 1], self.cmd('cms.query', 'cms1{t}', 'bar' + str(j)))
                self.assertEqual([1], self.cmd('cms.query', 'cms1{t}', 'foo' + str(j)))

        # Repeatedly delete cms3{t} from cms1{t} and verify content.
        # 'i' will increment by 2, and it will delete cms3{t} with weight -2
        for i in range(0, 10, 2):
            self.env.expect('cms.merge', 'cms1{t}', 2, 'cms1{t}', 'cms3{t}',
                            'weights', '1', '-2').ok()
            for j in range(0, 1000):
                self.assertEqual([8 - i], self.cmd('cms.query', 'cms1{t}', 'bar' + str(j)))
                self.assertEqual([1], self.cmd('cms.query', 'cms1{t}', 'foo' + str(j)))

        # Validate you can't delete cms3{t} again.
        self.env.expect('cms.merge', 'cms1{t}', 2, 'cms1{t}', 'cms3{t}',
                        'weights', '1', '-1').error().contains('CMS: MERGE overflow')

    def test_merge_overflow_large_cell(self):
        # Validate cms.merge fails if there is overflow while merging cells
        self.cmd('FLUSHALL')
        self.assertOk(self.cmd('cms.initbydim', 'cms1{t}', '2', '2'))
        self.assertOk(self.cmd('cms.initbydim', 'cms2{t}', '2', '2'))

        # 4000000000 will fit into 32-bit unsigned integer.
        self.assertEqual([4000000000], self.cmd('cms.incrby', 'cms1{t}', 'foo', '4000000000'))
        self.assertEqual([4000000000], self.cmd('cms.incrby', 'cms2{t}', 'foo', '4000000000'))

        self.env.expect('cms.merge', 'cms1{t}', 2, 'cms1{t}', 'cms2{t}',
                        'weights', '1', '-2').error().contains('CMS: MERGE overflow')
        self.env.expect('cms.merge', 'cms1{t}', 2, 'cms1{t}', 'cms2{t}',
                        'weights', '-2', '-2').error().contains('CMS: MERGE overflow')
        self.env.expect('cms.merge', 'cms1{t}', 2, 'cms1{t}', 'cms2{t}',
                        'weights', '-2', '1').error().contains('CMS: MERGE overflow')
        self.env.expect('cms.merge', 'cms1{t}', 2, 'cms1{t}', 'cms2{t}',
                        'weights', '2', '0').error().contains('CMS: MERGE overflow')
        self.env.expect('cms.merge', 'cms1{t}', 2, 'cms1{t}', 'cms2{t}',
                        'weights', '0', '2').error().contains('CMS: MERGE overflow')
        self.env.expect('cms.merge', 'cms1{t}', 2, 'cms1{t}', 'cms2{t}',
                        'weights', '-1', '-1').error().contains('CMS: MERGE overflow')

        # Validate keys did not change
        self.assertEqual(['width', 2, 'depth', 2, 'count', 4000000000],
                         self.cmd('cms.info', 'cms1{t}'))
        self.assertEqual(['width', 2, 'depth', 2, 'count', 4000000000],
                         self.cmd('cms.info', 'cms2{t}'))
        self.assertEqual([4000000000], self.cmd('cms.query', 'cms1{t}', 'foo'))
        self.assertEqual([4000000000], self.cmd('cms.query', 'cms2{t}', 'foo'))

    def test_merge_overflow_large_weight(self):
        # Validate cms.merge fails if there is overflow due to large weight arg
        self.cmd('FLUSHALL')
        self.assertOk(self.cmd('cms.initbydim', 'cms1{t}', '2', '2'))
        self.assertOk(self.cmd('cms.initbydim', 'cms2{t}', '2', '2'))
        self.assertEqual([4], self.cmd('cms.incrby', 'cms1{t}', 'foo', '4'))
        self.assertEqual([4], self.cmd('cms.incrby', 'cms2{t}', 'foo', '4'))

        # Test boundaries
        self.env.expect('cms.merge', 'cms1{t}', 2, 'cms1{t}', 'cms2{t}',
                        'weights', '9223372036854775807', '-4000000000').error().contains('CMS: MERGE overflow')
        self.env.expect('cms.merge', 'cms1{t}', 2, 'cms1{t}', 'cms2{t}',
                        'weights', '8000000000', '-4000000000').error().contains('CMS: MERGE overflow')
        self.env.expect('cms.merge', 'cms1{t}', 2, 'cms1{t}', 'cms2{t}',
                        'weights', '-8000000000', '4000000000').error().contains('CMS: MERGE overflow')
        self.env.expect('cms.merge', 'cms1{t}', 2, 'cms1{t}', 'cms2{t}',
                        'weights', '-800000000000', '8000000000').error().contains('CMS: MERGE overflow')
        self.env.expect('cms.merge', 'cms1{t}', 2, 'cms1{t}', 'cms2{t}',
                        'weights', '-1', '-1').error().contains('CMS: MERGE overflow')
        self.env.expect('cms.merge', 'cms1{t}', 2, 'cms1{t}', 'cms2{t}',
                        'weights', '-5', '-4').error().contains('CMS: MERGE overflow')

        # Validate keys did not change
        self.assertEqual(['width', 2, 'depth', 2, 'count', 4], self.cmd('cms.info', 'cms1{t}'))
        self.assertEqual(['width', 2, 'depth', 2, 'count', 4], self.cmd('cms.info', 'cms2{t}'))
        self.assertEqual([4], self.cmd('cms.query', 'cms1{t}', 'foo'))
        self.assertEqual([4], self.cmd('cms.query', 'cms2{t}', 'foo'))

        # An extreme test for a success scenario
        self.env.expect('cms.merge', 'cms1{t}', 2, 'cms1{t}', 'cms2{t}',
                        'weights', '-922337203685477500', '922337203685477502').ok()
        self.assertEqual(['width', 2, 'depth', 2, 'count', 8], self.cmd('cms.info', 'cms1{t}'))
        self.assertEqual([8], self.cmd('cms.query', 'cms1{t}', 'foo'))

    def test_watch(self):
        conn1 = self.env.getConnection()
        conn2 = self.env.getConnection()
        self.env.cmd('flushall')
        self.env.cmd('CMS.INITBYDIM', 'basecms1', '1000', '5')
        with conn1.pipeline() as pipe:
            pipe.watch('basecms1')
            conn2.execute_command('CMS.INCRBY', 'basecms1', 'smur', '5', 'rr', '9', 'ff', '99')
            pipe.multi()
            pipe.set('x', '1')
            try:
                pipe.execute()
                self.env.assertTrue(False, message='Multi transaction was not failed when it should have')
            except redis.exceptions.WatchError as e:
                self.env.assertContains('Watched variable changed', str(e))
    def test_insufficient_memory(self):
        self.cmd('FLUSHALL')
        self.env.expect('CMS.INITBYDIM',  'x', '4611686018427388100', '1').error().contains('CMS: Insufficient memory to create the key')
