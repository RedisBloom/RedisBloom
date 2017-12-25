#!/usr/bin/env python
from rmtest import ModuleTestCase
from redis import ResponseError


class CuckooTestCase(ModuleTestCase('../rebloom.so')):
    def test_count(self):
        c = self.client
        s = self.server

        self.cmd('CF.RESERVE', 'cf', '1000')
        self.assertEqual(0, self.cmd('cf.exists', 'cf', 'k1'))
        self.assertEqual(1, self.cmd('cf.add', 'cf', 'k1'))
        self.assertEqual(1, self.cmd('cf.add', 'cf', 'k1'))

        self.assertEqual(1, self.cmd('cf.exists', 'cf', 'k1'))
        self.assertEqual(2, self.cmd('cf.count', 'cf', 'k1'))

        # Delete the item
        self.assertEqual(1, self.cmd('cf.del', 'cf', 'k1'))
        self.assertEqual(1, self.cmd('cf.count', 'cf', 'k1'))
        self.assertEqual(1, self.cmd('cf.del', 'cf', 'k1'))
        self.assertEqual(0, self.cmd('cf.count', 'cf', 'k1'))
        self.assertEqual(0, self.cmd('cf.del', 'cf', 'k1'))

        for x in xrange(100):
            self.cmd('cf.add', 'nums', str(x))

        for x in xrange(100):
            self.assertEqual(1, self.cmd('cf.exists', 'nums', str(x)))

        for _ in c.retry_with_rdb_reload():
            for x in xrange(100):
                self.assertEqual(1, self.cmd('cf.exists', 'nums', str(x)))

    def test_aof(self):
        self.spawn_server(use_aof=True)
        # Ensure we have a pretty small filter
        self.cmd('cf.reserve', 'smallCF', 2)
        for x in xrange(1000):
            self.cmd('cf.add', 'smallCF', str(x))
        # Sanity check
        for x in xrange(1000):
            self.assertEqual(1, self.cmd('cf.exists', 'smallCF', str(x)))

        self.restart_and_reload()
        for x in xrange(1000):
            self.assertEqual(1, self.cmd('cf.exists', 'smallCF', str(x)))

    def test_setnx(self):
        self.assertEqual(1, self.cmd('cf.addnx', 'cf', 'k1'))
        self.assertEqual(0, self.cmd('cf.addnx', 'cf', 'k1'))
        self.assertEqual(1, self.cmd('cf.count', 'cf', 'k1'))
        self.assertEqual(1, self.cmd('cf.add', 'cf', 'k1'))
        self.assertEqual(2, self.cmd('cf.count', 'cf', 'k1'))


if __name__ == "__main__":
    import unittest
    unittest.main()
