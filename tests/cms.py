#!/usr/bin/env python
from rmtest import ModuleTestCase
from redis import ResponseError
import sys

if sys.version >= '3':
    xrange = range

class RebloomTestCase(ModuleTestCase('../rebloom.so')):
    def test_simple(self):
        self.assertOk(self.cmd('cms.initbydim', 'cms1', '20', '5'))
        self.assertOk(self.cmd('cms.incrby', 'cms1', 'a', '5'))
        self.assertEqual([5L], self.cmd('cms.query', 'cms1', 'a'))
        self.assertEqual(['width', 20L, 'depth', 5L, 'count', 5L], 
                         self.cmd('cms.info', 'cms1'))

        self.assertOk(self.cmd('cms.initbyprob', 'cms2', '0.1', '0.1'))
        self.assertOk(self.cmd('cms.incrby', 'cms2', 'a', '5'))
        self.assertEqual([5L], self.cmd('cms.query', 'cms2', 'a'))
        self.assertEqual(['width', 20L, 'depth', 4L, 'count', 5L], 
                         self.cmd('cms.info', 'cms2'))

    def test_validation(self):
        for args in (
            (),
            ('foo', ),
            ('foo', '0.1'),
            ('foo', '0.1', 'blah'),
            ('foo', '10'),
            ('foo', '10', 'blah'),
            ('foo', 'blah', '10'),
            ('foo', '0', '0'),
            ('foo', '0', '100'),
            ('foo', '100', '0'),
        ):
            self.assertRaises(ResponseError, self.cmd, 'cms.initbydim', *args)
            self.assertRaises(ResponseError, self.cmd, 'cms.initbyprob', *args)

        self.assertRaises(ResponseError, self.cmd, 'cms.initbydim', '0.1', '0.1')
        self.assertRaises(ResponseError, self.cmd, 'cms.initbyprob', '10', '10')

        self.assertOk(self.cmd('cms.initbydim', 'testDim', '100', '5'))
        self.assertOk(self.cmd('cms.initbyprob', 'testProb', '0.1', '0.1'))
    
        for args in ((), ('test',)):
            for cmd in ('cms.incrby', 'cms.query', 'cms.merge', 'cms.info'):
                self.assertRaises(ResponseError, self.cmd, cmd, *args)

    def test_incrby_query(self):
        self.cmd('cms.incrby', 'cms', 'bar', '5', 'baz', '42')
        self.assertEqual([0L], self.cmd('cms.query', 'cms', 'foo'))
        self.assertEqual([0, 5, 42], self.cmd('cms.query',
                                    'cms', 'foo', 'bar', 'baz'))
        self.assertRaises(ResponseError, self.cmd, 'cms.incrby',
                                    'cms', 'bar', '5', 'baz')
        self.assertEqual([0, 5, 42], self.cmd('cms.query',
                                    'cms', 'foo', 'bar', 'baz'))

        c = self.client
        self.assertOk(self.cmd('cms.incrby', 'test', 'foo', '1'))
        self.assertEqual([1L], self.cmd('cms.query', 'test', 'foo'))
        self.assertEqual([0L], self.cmd('cms.query', 'test', 'bar'))

        self.assertOk(self.cmd('cms.incrby', 'test', 'foo', '1', 'bar', '1'))
        for _ in c.retry_with_rdb_reload():
            self.assertEqual([2L], self.cmd('cms.query', 'test', 'foo'))
            self.assertEqual([1L], self.cmd('cms.query', 'test', 'bar'))
            self.assertEqual([0L], self.cmd('cms.query', 'test', 'nonexist'))
    
    def test_merge(self):
        self.cmd('cms.initbydim', 'small_1', '100', '5')
        self.cmd('cms.initbydim', 'small_2', '100', '5')
        self.cmd('cms.initbydim', 'small_3', '100', '5')
        self.cmd('cms.initbydim', 'large_4', '2000', '10')
        self.cmd('cms.initbydim', 'large_5', '2000', '10')
        self.cmd('cms.initbydim', 'large_6', '2000', '10')

        self.assertOk(self.cmd('cms.merge', 'small_3', 2, 'small_1', 'small_2'))
        self.assertOk(self.cmd('cms.merge', 'large_6', 2, 'large_4', 'large_5'))
        self.assertRaises(ResponseError, self.cmd, 'cms.merge', 'small_3', 2,
                                    'small_2', 'large_5')

'''
    def test_dump_and_load(self):
        # Store a filter
        self.cmd('bf.reserve', 'myBloom', '0.0001', '100')

        def do_verify():
            for x in xrange(1000):
                self.cmd('bf.add', 'myBloom', x)
                rv = self.cmd('bf.exists', 'myBloom', x)
                self.assertTrue(rv)
                rv = self.cmd('bf.exists', 'myBloom', 'nonexist_{}'.format(x))
                self.assertFalse(rv, x)

        do_verify()
        cmds = []
        cur = self.cmd('bf.scandump', 'myBloom', 0)
        first = cur[0]
        cmds.append(cur)

        while True:
            cur = self.cmd('bf.scandump', 'myBloom', first)
            first = cur[0]
            if first == 0:
                break
            else:
                cmds.append(cur)

        prev_info = self.cmd('bf.debug', 'myBloom')
        # Remove the filter
        self.cmd('del', 'myBloom')

        # Now, load all the commands:
        for cmd in cmds:
            self.cmd('bf.loadchunk', 'myBloom', *cmd)

        cur_info = self.cmd('bf.debug', 'myBloom')
        self.assertEqual(prev_info, cur_info)
        do_verify()

        # Try a bigger one
        self.cmd('del', 'myBloom')
        self.cmd('bf.reserve', 'myBloom', '0.0001', '10000000')
'''

if __name__ == "__main__":
    import unittest
    unittest.main()
