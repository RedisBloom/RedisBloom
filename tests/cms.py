#!/usr/bin/env python
from rmtest import ModuleTestCase
from redis import ResponseError
import sys

if sys.version >= '3':
    xrange = range


class RebloomTestCase(ModuleTestCase('../rebloom.so')):
    def test_custom_filter(self):
        # Can we create a client?
        c = self.client
        s = self.server

        self.assertOk(self.cmd('cms.initbydim', 'a', '20', '5'))
        self.assertOk(self.cmd('cms.incrby', 'a', 'a', '5'))
        self.assertEqual([5L], self.cmd('cms.query', 'a', 'a'))
        self.assertEqual(['width', 20L, 'depth', 5L, 'count', 5L], 
                         self.cmd('cms.info', 'a'))

        self.assertOk(self.cmd('cms.initbyprob', 'b', '0.1', '0.1'))
        self.assertOk(self.cmd('cms.incrby', 'b', 'a', '5'))
        self.assertEqual([5L], self.cmd('cms.query', 'b', 'a'))
        self.assertEqual(['width', 20L, 'depth', 4L, 'count', 5L], 
                         self.cmd('cms.info', 'b'))

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
            print args
            self.assertRaises(ResponseError, self.cmd, 'cms.initbydim', *args)
            self.assertRaises(ResponseError, self.cmd, 'cms.initbyprob', *args)

        self.assertRaises(ResponseError, self.cmd, 'cms.initbydim', '0.1', '0.1')
        self.assertRaises(ResponseError, self.cmd, 'cms.initbyprob', '10', '10')

        self.assertOk(self.cmd('cms.initbydim', 'testDim', '100', '5'))
        self.assertOk(self.cmd('cms.initbyprob', 'testProb', '0.1', '0.1'))

'''
        for args in ((), ('test',)):
            for cmd in ('bf.add', 'bf.madd', 'bf.exists', 'bf.mexists'):
                self.assertRaises(ResponseError, self.cmd, cmd, *args)

        for cmd in ('bf.exists', 'bf.add'):
            self.assertRaises(ResponseError, self.cmd, cmd, 'test', 1, 2)

    def test_    


        with self.assertResponseError():
            self.cmd('cms.info', 'test')
'''
        
'''
        self.assertEqual([1, 1, 1], self.cmd(
            'bf.madd', 'test', 'foo', 'bar', 'baz'))

        for _ in c.retry_with_rdb_reload():
            self.assertEqual(1, self.cmd('bf.exists', 'test', 'foo'))
            self.assertEqual(0, self.cmd('bf.exists', 'test', 'nonexist'))

        with self.assertResponseError():
            self.cmd('bf.reserve', 'test', '0.01')

        # Ensure there's no error when trying to add elements to it:
        for _ in c.retry_with_rdb_reload():
            self.assertEqual(0, self.cmd('bf.add', 'test', 'foo'))

    def test_set(self):
        c, s = self.client, self.server
        self.assertEqual(1, self.cmd('bf.add', 'test', 'foo'))
        self.assertEqual(1, self.cmd('bf.exists', 'test', 'foo'))
        self.assertEqual(0, self.cmd('bf.exists', 'test', 'bar'))

        # Set multiple keys at once:
        self.assertEqual([0, 1], self.cmd('bf.madd', 'test', 'foo', 'bar'))

        for _ in c.retry_with_rdb_reload():
            self.assertEqual(1, self.cmd('bf.exists', 'test', 'foo'))
            self.assertEqual(1, self.cmd('bf.exists', 'test', 'bar'))
            self.assertEqual(0, self.cmd('bf.exists', 'test', 'nonexist'))

    def test_multi(self):
        c, s = self.client, self.server
        self.assertEqual([1, 1, 1], self.cmd(
            'bf.madd', 'test', 'foo', 'bar', 'baz'))
        self.assertEqual([0, 1, 0], self.cmd(
            'bf.madd', 'test', 'foo', 'new1', 'bar'))

        self.assertEqual([0], self.cmd('bf.mexists', 'test', 'nonexist'))

        self.assertEqual([0, 1], self.cmd(
            'bf.mexists', 'test', 'nonexist', 'foo'))

    def test_validation(self):
        for args in (
            (),
            ('foo', ),
            ('foo', '0.001'),
            ('foo', '0.001', 'blah'),
            ('foo', '0', '0'),
            ('foo', '0', '100'),
            ('foo', 'blah', '1000')
        ):
            self.assertRaises(ResponseError, self.cmd, 'bf.reserve', *args)

        self.assertOk(self.cmd('bf.reserve', 'test', '0.01', '1000'))
        for args in ((), ('test',)):
            for cmd in ('bf.add', 'bf.madd', 'bf.exists', 'bf.mexists'):
                self.assertRaises(ResponseError, self.cmd, cmd, *args)

        for cmd in ('bf.exists', 'bf.add'):
            self.assertRaises(ResponseError, self.cmd, cmd, 'test', 1, 2)

    def test_oom(self):
        self.assertRaises(ResponseError, self.cmd, 'bf.reserve', 'test', 0.01, 4294967296)

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

    def test_missing(self):
        res = self.cmd('bf.exists', 'myBloom', 'foo')
        self.assertEqual(0, res)
        res = self.cmd('bf.mexists', 'myBloom', 'foo', 'bar', 'baz')
        self.assertEqual([0, 0, 0], res)

    def test_insert(self):
        with self.assertResponseError():
            self.cmd('bf.insert', 'missingFilter', 'NOCREATE', 'ITEMS', 'foo', 'bar')
        with self.assertResponseError():
            self.cmd('bf.insert', 'missingFilter')
        with self.assertResponseError():
            self.cmd('bf.insert', 'missingFilter', 'ITEMS')

        rep = self.cmd('BF.INSERT', 'missingFilter', 'ERROR',
                       '0.001', 'CAPACITY', '50000', 'ITEMS', 'foo')
        self.assertEqual([1], rep)
        self.assertEqual(['size:1', 'bytes:131072 bits:1048576 hashes:10 hashwidth:64 capacity:72931 size:1 ratio:0.001'],
                         [x.decode() for x in self.cmd('bf.debug', 'missingFilter')])

        rep = self.cmd('BF.INSERT', 'missingFilter', 'ERROR', '0.1', 'ITEMS', 'foo', 'bar', 'baz')
        self.assertEqual([0, 1, 1], rep)
        self.assertEqual(['size:3', 'bytes:131072 bits:1048576 hashes:10 hashwidth:64 capacity:72931 size:3 ratio:0.001'],
                         [x.decode() for x in self.cmd('bf.debug', 'missingFilter')])
'''

if __name__ == "__main__":
    import unittest
    unittest.main()
