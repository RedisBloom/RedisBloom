#!/usr/bin/env python
from rmtest import ModuleTestCase
from redis import ResponseError
import sys
from random import randint
import math

if sys.version >= '3':
    xrange = range

def TestSimple(self):
    self.assertOk(self.cmd('cms.reserve', 'cms1', '20'))
    self.assertOk(self.cmd('cms.incrby', 'cms1', 'a', '5'))
    self.assertEqual([5L], self.cmd('cms.query', 'cms1', 'a'))

def TestMergeExt(self):
    self.cmd('cms.reserve', 'A', '2000')
    self.cmd('cms.reserve', 'B', '2000')
    self.cmd('cms.reserve', 'C', '2000')
    
    itemsA = []
    itemsB = []
    for i in range(9000):
        itemsA.append(randint(0, 100))
        self.cmd('cms.incrby', 'A', i, itemsA[i])
        itemsB.append(randint(0, 100))
        self.cmd('cms.incrby', 'B', i, itemsB[i])    
    self.assertOk(self.cmd('cms.merge', 'C', 2, 'A', 'B'))
    for i in range(9000):
        print(i, itemsA[i], self.cmd('cms.query', 'A', i), itemsB[i], self.cmd('cms.query', 'B', i), self.cmd('cms.query', 'C', i))
#       print(itemsA[i], itemsB[i], self.cmd('cms.query', 'C', i))

class CMSTest(ModuleTestCase('../rebloom.so')):
    def test_simple(self):
        self.assertOk(self.cmd('cms.reserve', 'cms1', '20'))
        self.assertOk(self.cmd('cms.incrby', 'cms1', 'a', '5'))
        self.assertEqual([5L], self.cmd('cms.query', 'cms1', 'a'))
        self.assertEqual(['capacity', 20L, 'width', 54L, 'depth', 5L, 'count', 5L, 'fill rate %', '5'], 
                         self.cmd('cms.info', 'cms1'))

    def test_validation(self):
        for args in (
            (),
            ('foo', ),
            ('foo', '0.1'),
            ('foo', 'blah'),
            ('foo', '0'),
            ('foo', '100', '100'),
            ('foo', '100', '0.1'),
            ('foo', '0.1', '100'),
            ('foo', '0.1', '0.1'),
            ('foo', '0.1', 'probability', '0.1'),
            ('foo', '100', 'probability', '100'),
            ('foo', '0.1', 'probability', '100'),     
            ('foo', '100', '100', 'probability'),
        ):
            self.assertRaises(ResponseError, self.cmd, 'cms.reserve', *args)

        self.assertOk(self.cmd('cms.reserve', 'testDim', '100'))
        self.assertOk(self.cmd('cms.reserve', 'testProb1', '100', 'probability', '0.01'))
        self.assertEqual(['capacity', 100L, 'width', 250L, 'depth', 5L, 'count', 0L, 'fill rate %', '0'], 
                         self.cmd('cms.info', 'testProb1'))
        self.assertOk(self.cmd('cms.reserve', 'testProb2', '100', 'probability', '0.03'))
        self.assertEqual(['capacity', 100L, 'width', 189L, 'depth', 5L, 'count', 0L, 'fill rate %', '0'], 
                         self.cmd('cms.info', 'testProb2'))
        self.assertOk(self.cmd('cms.reserve', 'testProb3', '100', 'probability', '0.001'))
        self.assertEqual(['capacity', 100L, 'width', 444L, 'depth', 5L, 'count', 0L, 'fill rate %', '0'], 
                         self.cmd('cms.info', 'testProb3'))

        for args in ((), ('test',)):
            for cmd in ('cms.incrby', 'cms.query', 'cms.merge', 'cms.info'):
                self.assertRaises(ResponseError, self.cmd, cmd, *args)
    
    def test_incrby_query(self):
        self.assertOk(self.cmd('cms.reserve', 'cms', '100'))
        self.cmd('cms.incrby', 'cms', 'bar', '5', 'baz', '42')
        self.assertEqual([0], self.cmd('cms.query', 'cms', 'foo'))
        self.assertEqual([0, 5, 42], self.cmd('cms.query',
                                    'cms', 'foo', 'bar', 'baz'))
        self.assertRaises(ResponseError, self.cmd, 'cms.incrby',
                                    'cms', 'bar', '5', 'baz')
        self.assertEqual([0, 5, 42], self.cmd('cms.query',
                                    'cms', 'foo', 'bar', 'baz'))

        c = self.client
        self.assertOk(self.cmd('cms.reserve', 'test', '100'))
        self.assertOk(self.cmd('cms.incrby', 'test', 'foo', '1'))
        self.assertEqual([1], self.cmd('cms.query', 'test', 'foo'))
        self.assertEqual([0], self.cmd('cms.query', 'test', 'bar'))

        self.assertOk(self.cmd('cms.incrby', 'test', 'foo', '1', 'bar', '1'))
        for _ in c.retry_with_rdb_reload():
            self.assertEqual([2], self.cmd('cms.query', 'test', 'foo'))
            self.assertEqual([1], self.cmd('cms.query', 'test', 'bar'))
            self.assertEqual([0], self.cmd('cms.query', 'test', 'nonexist'))
    
    def test_merge(self):
        self.cmd('cms.reserve', 'small_1', '20')
        self.cmd('cms.reserve', 'small_2', '20')
        self.cmd('cms.reserve', 'small_3', '20')
        self.cmd('cms.reserve', 'large_4', '2000')
        self.cmd('cms.reserve', 'large_5', '2000')
        self.cmd('cms.reserve', 'large_6', '2000')

        # empty small batch
        self.assertOk(self.cmd('cms.merge', 'small_3', 2, 'small_1', 'small_2'))
        self.assertEqual(['capacity', 20L, 'width', 54L, 'depth', 5L, 'count', 0L, 'fill rate %', '0'], 
                         self.cmd('cms.info', 'small_3'))

        # empty large batch
        self.assertOk(self.cmd('cms.merge', 'large_6', 2, 'large_4', 'large_5'))
        self.assertEqual(['capacity', 2000L, 'width', 5400L, 'depth', 5L, 'count', 0L, 'fill rate %', '0'], 
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
    
    def test_merge_extensive(self):
        self.cmd('cms.reserve', 'A', '2000')
        self.cmd('cms.reserve', 'B', '2000')
        self.cmd('cms.reserve', 'C', '2000')
        
        itemsA = []
        itemsB = []
        for i in range(10000):
            itemsA.append(randint(0, 100))
            self.cmd('cms.incrby', 'A', str(i), itemsA[i])
            itemsB.append(randint(0, 100))
            self.cmd('cms.incrby', 'B', str(i), itemsB[i])    
        self.assertOk(self.cmd('cms.merge', 'C', 2, 'A', 'B'))

#        for i in range(10000):
#            print(i, itemsA[i], self.cmd('cms.query', 'A', i), itemsB[i], self.cmd('cms.query', 'B', i), self.cmd('cms.query', 'C', i))
#            print(itemsA[i], itemsB[i], self.cmd('cms.query', 'C', i))
#        print(self.cmd('cms.info', 'A'))
#        print(self.cmd('cms.info', 'B'))
#        print(self.cmd('cms.info', 'C'))

if __name__ == "__main__":
    import unittest
    unittest.main()
