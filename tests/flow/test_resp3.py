from common import *

class testResp3():
    def __init__(self):
        self.env = Env(protocol=3)

    def test_bf_resp3(self):
        env = self.env
        env.cmd('FLUSHALL')
        res = env.cmd('bf.add', 'test', 'item')
        assert res == True

        res = env.cmd('bf.card', 'test')
        assert res == 1

        res = env.cmd('bf.EXISTS', 'test', 'item')
        assert res == True

        res = env.cmd('bf.info', 'test')
        assert res == \
        {b'Capacity': 100, b'Size': 240, b'Number of filters': 1,
         b'Number of items inserted': 1, b'Expansion rate': 2}

        res = env.cmd('bf.insert', 'test', 'ITEMS', 'item2', 'item3')
        assert res == [True, True]

        res = env.cmd('bf.madd', 'test', 'item4', 'item5')
        assert res == [True, True]

        res = env.cmd('bf.MEXISTS', 'test', 'item4', 'item5', 'item6')
        assert res == [True, True, False]

        env.cmd('CF.RESERVE a 4 EXPANSION 0')
        env.assertEqual(env.cmd('CF.ADD a 3'), 1)
        env.assertEqual(env.cmd('CF.ADD a 4'), 1)
        env.assertEqual(env.cmd('CF.del a 3'), True)
        env.assertEqual(env.cmd('CF.del a 3'), False)
        env.assertEqual(env.cmd('CF.EXISTS a 3'), False)
        env.assertEqual(env.cmd('CF.EXISTS a 4'), True)
        env.assertEqual(env.cmd('CF.MEXISTS a 4 3'), [True, False])

        res = env.cmd('cf.info a')
        assert res == {b'Size': 60, b'Number of buckets': 2,
                       b'Number of filters': 1, b'Number of items inserted': 1,
                       b'Number of items deleted': 1, b'Bucket size': 2,
                       b'Expansion rate': 0, b'Max iterations': 20}

        env.assertEqual(env.cmd('cms.initbyprob', 'cms2', '0.001', '0.01'), b'OK')
        env.assertEqual([5], env.cmd('cms.incrby', 'cms2', 'a', '5'))
        env.assertEqual([5], env.cmd('cms.query', 'cms2', 'a'))
        env.assertEqual({b'width': 2000, b'depth': 7, b'count': 5},
                         env.cmd('cms.info', 'cms2'))

        env.assertEqual(env.cmd("tdigest.create", "tdigest"), b'OK')
        # independent of the datapoints this sketch has an invariant size after creation
        for x in range(1, 10001):
            env.assertEqual(env.cmd("tdigest.add", "tdigest", x), b'OK')
        td_info = env.cmd("tdigest.info", "tdigest")
        assert td_info == {b'Compression': 100, b'Capacity': 610,
                           b'Merged nodes': 51, b'Unmerged nodes': 422,
                           b'Merged weight': 9578, b'Unmerged weight': 422,
                           b'Observations': 10000, b'Total compressions': 17,
                           b'Memory usage': 9768}

        env.cmd('topk.reserve', 'topk', '2', '50', '5', '0.9')
        env.cmd('topk.add', 'topk', 'foo', 'bar', 'baz', '42', 'foo', 'bar', 'baz', )
        env.cmd('topk.add', 'topk', 'foo', 'baz', '42', 'foo', 'baz')
        env.cmd('topk.add', 'topk', 'foo', 'bar', 'baz', 'foo', 'baz')
        info = env.cmd('topk.info', 'topk')
        assert info == {b'k': 2, b'width': 50, b'depth': 5, b'decay': 0.9}

        env.assertEqual([True, False, True, False], env.cmd('topk.query',
                                        'topk', 'foo', 'bar', 'baz', 'dan'))
