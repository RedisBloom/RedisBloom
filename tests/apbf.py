#!/usr/bin/env python
from rmtest import ModuleTestCase
from redis import ResponseError
import sys
import time

if sys.version >= '3':
    xrange = range

class APBFTest(ModuleTestCase('../redisbloom.so')):
    def test_simple(self):
        self.cmd('flushall')
        items_list = ['AGE', 'PARTITIONED', 'BLOOM', 'FILTER']
        self.assertOk(self.cmd('apbfc.reserve apbf 2 1000'))
        for i in range(len(items_list)):
            self.assertEqual('OK', self.cmd('apbfc.add apbf', items_list[i]))
        for i in range(len(items_list)):
            self.assertEqual([1L], self.cmd('apbfc.exists apbf', items_list[i]))
        self.assertEqual([0L], self.cmd('apbfc.exists apbf bloom'))
        self.assertEqual(1L, self.cmd('del apbf'))

        self.assertOk(self.cmd('apbft.reserve apbf 2 1000 10'))
        for i in range(len(items_list)):
            self.assertEqual('OK', self.cmd('apbft.add apbf', items_list[i]))
        for i in range(len(items_list)):
            self.assertEqual([1L], self.cmd('apbft.exists apbf', items_list[i]))
        self.assertEqual([0L], self.cmd('apbft.exists apbf bloom'))
    
    def test_expiry_count(self):
        tests = 10000
        self.cmd('flushall')
        self.assertOk(self.cmd('apbfc.reserve apbf 2', tests/10))
        for i in range(tests * 2):
            self.assertEqual('OK', self.cmd('apbfc.add apbf', i))
        for i in range(int(tests * 1.9), tests * 2):
            self.assertEqual([1L], self.cmd('apbfc.exists apbf', str(i)))

        positive = 0.0
        for i in range(0, int(tests)):
            positive += self.cmd('apbfc.exists apbf', i)[0]
        self.assertLess(positive / tests, 0.015)

    def test_expiry_time(self):
        tests = 1000
        self.cmd('flushall')
        self.assertOk(self.cmd('apbft.reserve apbf 2', tests/10, 1))
        for i in range(tests):
            self.assertEqual('OK', self.cmd('apbft.add apbf', i))
        for i in range(tests):
            self.assertEqual([1L], self.cmd('apbft.exists apbf', str(i)))

        time.sleep(.5)
        positive = 0.0
        for i in range(0, int(tests)):
            positive += self.cmd('apbfc.exists apbf', i)[0]
        self.assertEqual(1, positive / tests)

        # test empty after time expiry
        time.sleep(.5)
        for i in range(tests):
            self.assertEqual([1L], self.cmd('apbft.exists apbf', str(i)))
    
        for i in range (100):
            self.assertEqual('OK', self.cmd('apbft.add apbf', i))
            if i > 10:
                for j in range(10):
                    self.assertEqual([1L], self.cmd('apbft.exists apbf', str(i - j)))

    def test_info(self):
        self.cmd('flushall')
        self.assertOk(self.cmd('apbfc.reserve apbfc 2 1000'))
        results = ['Type', 'Age Partitioned Bloom Filter - Count',
                   'Size', 4024L, 'Capacity', 1000L, 'Error rate', 2L, 
                   'Inserts count', 0L, 'Hash functions count', 10L,
                   'Periods count', 25L, 'Slices count', 35L, 'Time Span', 0L]
        self.assertEqual(results, self.cmd('apbfc.info apbfc'))

        self.assertOk(self.cmd('apbft.reserve apbft 2 1000 10'))
        results = ['Type', 'Age Partitioned Bloom Filter - Time',
                   'Size', 4024, 'Capacity', 1000L, 'Error rate', 2L, 
                   'Inserts count', 0L, 'Hash functions count', 10L,
                   'Periods count', 25L, 'Slices count', 35L, 'Time Span', 10000L]
        self.assertEqual(results, self.cmd('apbfc.info apbft'))

    def test_rdb(self):
        tests = 1000
        self.cmd('flushall')
        self.assertOk(self.cmd('apbfc.reserve apbf 2', tests))
        for i in range(tests * 3):
            self.assertEqual('OK', self.cmd('apbfc.add apbf', i))
        for i in range(int(tests * 2.1), tests * 3):
            self.assertEqual(1, self.cmd('apbfc.exists apbf', i)[0])

        self.client.retry_with_rdb_reload()
        self.client.dr.dump_and_reload()
        for i in range(int(tests * 2.1), tests * 3):
            self.assertEqual(1, self.cmd('apbfc.exists apbf', i)[0])

    def test_args_count(self):
        # Count type
        self.assertRaises(ResponseError, self.cmd, 'apbfc.reserve apbf')
        self.assertRaises(ResponseError, self.cmd, 'apbfc.add apbf')
        self.assertRaises(ResponseError, self.cmd, 'apbfc.exists apbf')
        self.assertRaises(ResponseError, self.cmd, 'apbfc.info')

        # other key type
        self.cmd('set foo bar')
        self.assertRaises(ResponseError, self.cmd, 'apbfc.reserve foo 3 1000')
        self.assertRaises(ResponseError, self.cmd, 'apbfc.add foo bar')
        self.assertRaises(ResponseError, self.cmd, 'apbfc.exists foo bar')
        
        # key does not exist
        self.assertRaises(ResponseError, self.cmd, 'apbfc.add bar foo')
        self.assertRaises(ResponseError, self.cmd, 'apbfc.exists bar foo')

    def test_args_time(self):
        # Time type
        self.assertRaises(ResponseError, self.cmd, 'apbft.reserve apbf')
        self.assertRaises(ResponseError, self.cmd, 'apbft.add apbf')
        self.assertRaises(ResponseError, self.cmd, 'apbft.exists apbf')
        self.assertRaises(ResponseError, self.cmd, 'apbft.info')

        # other key type
        self.cmd('set foo bar')
        self.assertRaises(ResponseError, self.cmd, 'apbft.reserve foo 3 1000')
        self.assertRaises(ResponseError, self.cmd, 'apbft.add foo bar')
        self.assertRaises(ResponseError, self.cmd, 'apbft.exists foo bar')
        
        # key does not exist
        self.assertRaises(ResponseError, self.cmd, 'apbft.add bar foo')
        self.assertRaises(ResponseError, self.cmd, 'apbft.exists bar foo')

if __name__ == "__main__":
    import unittest
    unittest.main()
