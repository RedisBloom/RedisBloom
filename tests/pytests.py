#!/usr/bin/env python
from rmtest import ModuleTestCase
from redis import ResponseError
import sys

if sys.version >= '3':
    xrange = range

class RebloomTestCase(ModuleTestCase('../redisbloom.so')):
    def test_custom_filter(self):
        # Can we create a client?
        c = self.client
        s = self.server

        self.assertOk(self.cmd('bf.reserve', 'test', '0.05', '1000'))
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

        self.cmd('set', 'foo', 'bar')
        self.assertRaises(ResponseError, self.cmd, 'bf.reserve', 'foo')

    def test_oom(self):
        self.assertRaises(ResponseError, self.cmd, 'bf.reserve', 'test', 0.01, 4294967296)
    
    def test_rdb_reload(self):
        self.assertEqual(1, self.cmd('bf.add', 'test', 'foo'))
        c = self.client   
        yield 1 
        c.dr.dump_and_reload()
        yield 2
        self.assertEqual(1, self.cmd('bf.exists', 'test', 'foo'))
        self.assertEqual(0, self.cmd('bf.exists', 'test', 'bar'))
    
    def test_dump_and_load(self):
        # Store a filter
        self.cmd('bf.reserve', 'myBloom', '0.0001', '1000')

        # test is probabilistic and might fail. It is OK to change variables if 
        # certain to not break anything
        def do_verify():
            for x in xrange(1000):
                self.cmd('bf.add', 'myBloom', x)
                rv = self.cmd('bf.exists', 'myBloom', x)
                self.assertTrue(rv)
                rv = self.cmd('bf.exists', 'myBloom', 'nonexist_{}'.format(x))
                self.assertFalse(rv, x)

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

        rep = self.cmd('BF.INSERT', 'missingFilter', 'ERROR',
                       '0.001', 'CAPACITY', '50000', 'ITEMS', 'foo')
        self.assertEqual([1], rep)
        self.assertEqual(['size:1', 'bytes:131072 bits:1048576 hashes:10 hashwidth:64 capacity:72931 size:1 ratio:0.001'],
                         [x.decode() for x in self.cmd('bf.debug', 'missingFilter')])

        rep = self.cmd('BF.INSERT', 'missingFilter', 'ERROR', '0.1', 'ITEMS', 'foo', 'bar', 'baz')
        self.assertEqual([0, 1, 1], rep)
        self.assertEqual(['size:3', 'bytes:131072 bits:1048576 hashes:10 hashwidth:64 capacity:72931 size:3 ratio:0.001'],
                         [x.decode() for x in self.cmd('bf.debug', 'missingFilter')])

    def test_mem_usage(self):
        self.assertOk(self.cmd('bf.reserve', 'bf', '0.05', '1000'))
        self.assertEqual(1148, self.cmd('MEMORY USAGE', 'bf'))
        self.assertEqual([1, 1, 1], self.cmd(
            'bf.madd', 'bf', 'foo', 'bar', 'baz'))
        self.assertEqual(1148, self.cmd('MEMORY USAGE', 'bf'))
        with self.assertResponseError():
            self.cmd('bf.debug', 'bf', 'noexist')
        with self.assertResponseError():
            self.cmd('bf.debug', 'cf')

    def test_debug(self):
        self.assertOk(self.cmd('bf.reserve', 'bf', '0.01', '10'))
        for i in range(100):
            self.cmd('bf.add', 'bf', str(i))
        self.assertEqual(self.cmd('bf.debug', 'bf'), ['size:99',
                'bytes:16 bits:128 hashes:7 hashwidth:64 capacity:13 size:13 ratio:0.01',
                'bytes:64 bits:512 hashes:9 hashwidth:64 capacity:40 size:40 ratio:0.0025',
                'bytes:256 bits:2048 hashes:12 hashwidth:64 capacity:121 size:46 ratio:0.0003125'])

        self.cmd('del', 'bf')
        
        self.assertOk(self.cmd('bf.reserve', 'bf', '0.001', '100'))
        for i in range(1000):
            self.cmd('bf.add', 'bf', str(i))
        self.assertEqual(self.cmd('bf.debug', 'bf'), ['size:1000',
                'bytes:256 bits:2048 hashes:10 hashwidth:64 capacity:142 size:142 ratio:0.001',
                'bytes:1024 bits:8192 hashes:12 hashwidth:64 capacity:474 size:474 ratio:0.00025',
                'bytes:4096 bits:32768 hashes:15 hashwidth:64 capacity:1517 size:384 ratio:3.125e-05'])

if __name__ == "__main__":
    import unittest
    unittest.main()
