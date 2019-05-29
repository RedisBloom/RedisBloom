#!/usr/bin/env python
from rmtest import ModuleTestCase
from redis import ResponseError
import sys
from random import randint
import math

if sys.version >= '3':
    xrange = range


class TopKTest(ModuleTestCase('/home/ariel/redis/bloom/rebloom.so')):
    def test_simple(self):
        self.assertOk(self.cmd('topk.reserve', 'topk', '20', '50', '5', '0.9'))
        self.assertOk(self.cmd('topk.add', 'topk', 'a'))
        self.assertOk(self.cmd('topk.add', 'topk', 'a', 'b'))
        
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
    
    def test_add_query_count(self):
        self.assertOk(self.cmd('topk.reserve', 'topk', '20', '50', '5', '0.9'))

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
        
        c = self.client
        self.assertOk(self.cmd('topk.add', 'topk', 'foo'))
        self.assertEqual([1], self.cmd('topk.query', 'topk', 'foo'))
        self.assertEqual([0], self.cmd('topk.query', 'topk', 'xyxxy'))

        self.assertOk(self.cmd('topk.add', 'topk', 'foo', '1', 'bar', '1'))
        for _ in c.retry_with_rdb_reload():
            self.assertEqual([2], self.cmd('topk.count', 'topk', 'foo'))
            self.assertEqual([3L], self.cmd('topk.count', 'topk', 'bar'))
            self.assertEqual([0], self.cmd('topk.count', 'topk', 'nonexist'))
        

    def test_list_info(self):
        self.cmd('topk.reserve', 'topk', '2', '50', '5', '0.9')
        self.cmd('topk.add', 'topk', 'foo', 'bar', 'baz', '42', 'foo', 'bar', 'baz',)
        self.cmd('topk.add', 'topk', 'foo', 'baz', '42', 'foo', 'baz',)
        self.cmd('topk.add', 'topk', 'foo', 'bar', 'baz', 'foo', 'baz',)
     
        self.assertEqual(['foo', 'baz'], self.cmd('topk.list', 'topk'))

        info = self.cmd('topk.info', 'topk')
        self.assertEqual('k', info[0])
        self.assertEqual(2, info[1])
        self.assertEqual('width', info[2])
        self.assertEqual(50, info[3])
        self.assertEqual('depth', info[4])
        self.assertEqual(5, info[5])
        self.assertEqual('decay', info[6])
        self.assertAlmostEqual(0.9, float(info[7]))


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
        self.assertEqual(50, res)


    
if __name__ == "__main__":
    import unittest
    unittest.main()
