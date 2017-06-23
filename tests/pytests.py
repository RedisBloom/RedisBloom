#!/usr/bin/env python
from rmtest import ModuleTestCase
from redis import ResponseError


class RebloomTestCase(ModuleTestCase('../rebloom.so')):
    def setUp(self):
        super(RebloomTestCase, self).setUp()
        self.server = self.redis()
        self.server.start()
        self.client = self.server.client()

    def tearDown(self):
        self.server.stop()
        self.server = None
        super(RebloomTestCase, self).tearDown()

    def cmd(self, *args, **kw):
        return self.client.execute_command(*args, **kw)

    def test_static_filter(self):
        # Can we create a client?
        c = self.client
        s = self.server

        self.assertOk(self.cmd('bf.create', 'test',
                               '0.01', 'foo', 'bar', 'baz'))

        for _ in c.retry_with_rdb_reload():
            self.assertEqual(1, self.cmd('bf.test', 'test', 'foo'))
            self.assertEqual(0, self.cmd('bf.test', 'test', 'nonexist'))

        with self.assertResponseError():
            self.cmd('bf.create', 'test', '0.01', 'foo', 'bar', 'baz')

        # Ensure there's an error when trying to add elements to it:
        for _ in c.retry_with_rdb_reload():
            with self.assertResponseError():
                self.cmd('bf.set', 'test', 'foo')

    def test_set(self):
        c, s = self.client, self.server
        self.assertOk(self.cmd('bf.set', 'test', 'foo'))
        self.assertEqual(1, self.cmd('bf.test', 'test', 'foo'))
        self.assertEqual(0, self.cmd('bf.test', 'test', 'bar'))

        # Set multiple keys at once:
        self.assertOk(self.cmd('bf.set', 'test', 'foo', 'bar'))

        for _ in c.retry_with_rdb_reload():
            self.assertEqual(1, self.cmd('bf.test', 'test', 'foo'))
            self.assertEqual(1, self.cmd('bf.test', 'test', 'bar'))
            self.assertEqual(0, self.cmd('bf.test', 'test', 'nonexist'))

    def test_setnx(self):
        c, s = self.client, self.server
        self.assertOk(self.cmd('bf.setnx', 'test', 'foo'))
        self.assertRaises(ResponseError, self.cmd, 'bf.setnx', 'test', 'bar')
        self.assertEqual(1, self.cmd('bf.test', 'test', 'foo'))
        self.assertEqual(0, self.cmd('bf.test', 'test', 'bar'))
