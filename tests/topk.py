#!/usr/bin/env python
from rmtest import ModuleTestCase
from redis import ResponseError
import sys
from random import randint
import math

if sys.version >= '3':
    xrange = range


class TopKTest(ModuleTestCase('../redisbloom.so')):
    def test_simple(self):
        self.assertOk(self.cmd('topk.reserve', 'topk', '20', '50', '5', '0.9'))
        self.assertEqual([None], self.cmd('topk.add', 'topk', 'a'))
        self.assertEqual([None, None], self.cmd('topk.add', 'topk', 'a', 'b'))
        
        self.assertEqual([1L], self.cmd('topk.query', 'topk', 'a'))
        self.assertEqual([0L], self.cmd('topk.query', 'topk', 'c'))

    def test_validation(self):
        for args in (
            (),
            ('foo', ),
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
        self.assertOk(self.cmd('topk.reserve', 'topk', '20', '50', '5', '0.9'))
        self.client.dr.dump_and_reload()
        
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
        self.client.retry_with_rdb_reload()
        self.assertEqual([2], self.cmd('topk.count', 'topk', 'foo'))
        self.assertEqual([3L], self.cmd('topk.count', 'topk', 'bar'))
        self.assertEqual([0], self.cmd('topk.count', 'topk', 'nonexist'))

    def test_incrby(self):
        self.assertOk(self.cmd('topk.reserve', 'topk', '3', '10', '3', '1'))
        self.assertEqual([None, None, None], self.cmd('topk.incrby', 'topk', 'bar', 3, 'baz', 6, '42', 2))
        self.assertEqual([None, 'bar'], self.cmd('topk.incrby', 'topk', '42', 8, 'xyzzy', 4))
        self.assertEqual([3, 6, 10, 4, 0], self.cmd('topk.count', 'topk', 'bar', 'baz', '42', 'xyzzy', 4))
        self.assertRaises(ResponseError, self.cmd, 'topk.incrby')
        self.assertTrue(isinstance(self.cmd('topk.incrby', 'topk', 'foo', -5)[0], ResponseError))

    def test_lookup_table(self):
        self.assertOk(self.cmd('topk.reserve', 'topk', '1', '3', '3', '.9'))
        self.cmd('topk.incrby', 'topk', 'bar', 300, 'baz', 600, '42', 200)
        self.cmd('topk.incrby', 'topk', '42', 80, 'xyzzy', 400)
        self.assertEqual(['baz'], self.cmd('topk.list', 'topk'))

    def test_list_info(self):
        self.cmd('topk.reserve', 'topk', '2', '50', '5', '0.9')
        self.assertRaises(ResponseError, self.cmd, 'topk.reserve', 'topk', '2', '50', '5', '0.9')        
        self.cmd('topk.add', 'topk', 'foo', 'bar', 'baz', '42', 'foo', 'bar', 'baz',)
        self.cmd('topk.add', 'topk', 'foo', 'baz', '42', 'foo', 'baz',)
        self.cmd('topk.add', 'topk', 'foo', 'bar', 'baz', 'foo', 'baz',)
        self.assertEqual(['foo', 'baz'], self.cmd('topk.list', 'topk'))
        self.assertEqual([None, None, None, 'foo', None], 
                         self.cmd('topk.add', 'topk', 'bar', 'bar', 'bar', 'bar', 'bar'))
        self.assertEqual(['baz', 'bar'], self.cmd('topk.list', 'topk'))

        self.assertRaises(ResponseError, self.cmd, 'topk.list', 'topk', '_topk_')        

        info = self.cmd('topk.info', 'topk')
        self.assertEqual('k', info[0])
        self.assertEqual(2, info[1])
        self.assertEqual('width', info[2])
        self.assertEqual(50, info[3])
        self.assertEqual('depth', info[4])
        self.assertEqual(5, info[5])
        self.assertEqual('decay', info[6])
        self.assertAlmostEqual(0.9, float(info[7]))
        self.assertRaises(ResponseError, self.cmd, 'topk.info', 'topk', '_topk_')        

        self.client.dr.dump_and_reload()
        self.cmd('topk.reserve', 'test', '3', '50', '5', '0.9')
        self.cmd('topk.add', 'test', 'foo')
        self.assertEqual([None, 'foo', None], self.cmd('topk.list', 'test'))
        self.assertEqual(4190, self.cmd('MEMORY USAGE', 'test'))

    def test_time(self):
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
        res = sum(1 for i in range(len(heapList)) if int(heapList[i]) % 100 == 0)
        self.assertGreater(res, 45)
    
if __name__ == "__main__":
    import unittest
    unittest.main()
