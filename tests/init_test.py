from rmtest import ModuleTestCase
from redis import ResponseError
import sys

if sys.version >= '3':
    xrange = range
class InitTestEmpty(ModuleTestCase('../redisbloom.so', module_args=[""])):
    def test_fake(self):
        c, s = self.client, self.server
        self.assertOk('OK', self.cmd('set', 'test', 'foo'))

class InitTestSize(ModuleTestCase('../redisbloom.so', module_args=['INITIAL_SIZE', '400'])):
    def test_fake(self):
        c, s = self.client, self.server
        self.assertOk('OK', self.cmd('set', 'test', 'foo'))

class InitTestError(ModuleTestCase('../redisbloom.so', module_args=['ERROR_RATE', '0.1'])):
    def test_fake(self):
        c, s = self.client, self.server
        self.assertOk('OK', self.cmd('set', 'test', 'foo'))

class InitTestCFMaxExp(ModuleTestCase('../redisbloom.so', module_args=['CF_MAX_EXPANSIONS', '512'])):
    def test_fake(self):
        c, s = self.client, self.server
        self.assertOk('OK', self.cmd('set', 'test', 'foo'))

class InitTestCaseFailMissingArgs(ModuleTestCase('../redisbloom.so', module_args=['ONE_VAR'])):
    def test_init_args(self):
        try:
            c, s = self.client, self.server
        except Exception:
            delattr(self, '_server')
            self.assertOk('OK')
        else:
            self.assertOk('NotOK')

class InitTestCaseFailWrongArgs(ModuleTestCase('../redisbloom.so', module_args=['MADEUP', 'VAR'])):
    def test_init_args(self):
        try:
            c, s = self.client, self.server
        except Exception:
            delattr(self, '_server')
            self.assertOk('OK')
        else:
            self.assertOk('NotOK')
class InitTestCaseFailSize(ModuleTestCase('../redisbloom.so', module_args=['INITIAL_SIZE', '-1'])):
    def test_init_args(self):
        try:
            c, s = self.client, self.server
        except Exception:
            delattr(self, '_server')
            self.assertOk('OK')
        else:
            self.assertOk('NotOK')

class InitTestCaseFailSizeStr(ModuleTestCase('../redisbloom.so', module_args=['INITIAL_SIZE', 'BF'])):
    def test_init_args(self):
        try:
            c, s = self.client, self.server
        except Exception:
            delattr(self, '_server')
            self.assertOk('OK')
        else:
            self.assertOk('NotOK')

class InitTestCaseFailError(ModuleTestCase('../redisbloom.so', module_args=['ERROR_RATE', '-1'])):
    def test_init_args(self):
        try:
            c, s = self.client, self.server
        except Exception:
            delattr(self, '_server')
            self.assertOk('OK')
        else:
            self.assertOk('NotOK')

class InitTestCaseFailErrorStr(ModuleTestCase('../redisbloom.so', module_args=['ERROR_RATE', 'BF'])):
    def test_init_args(self):
        try:
            c, s = self.client, self.server
        except Exception:
            delattr(self, '_server')
            self.assertOk('OK')
        else:
            self.assertOk('NotOK')

class InitTestCaseFailCFMaxExp(ModuleTestCase('../redisbloom.so', module_args=['CF_MAX_EXPANSIONS', '-1'])):
    def test_init_args(self):
        try:
            c, s = self.client, self.server
        except Exception:
            delattr(self, '_server')
            self.assertOk('OK')
        else:
            self.assertOk('NotOK')

class InitTestCaseFailCFMaxExpStr(ModuleTestCase('../redisbloom.so', module_args=['CF_MAX_EXPANSIONS', 'CF'])):
    def test_init_args(self):
        try:
            c, s = self.client, self.server
        except Exception:
            delattr(self, '_server')
            self.assertOk('OK')
        else:
            self.assertOk('NotOK')

if __name__ == "__main__":
    import unittest
    unittest.main()
