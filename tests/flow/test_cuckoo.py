#!/usr/bin/env python3
import os

from RLTest import Env
from redis import ResponseError

xrange = range

is_valgrind = True if ('VGD' in os.environ or 'VALGRIND' in os.environ) else False


class testCuckoo():
    def __init__(self):
        self.env = Env(decodeResponses=True)
        self.assertOk = self.env.assertTrue
        self.cmd = self.env.cmd
        self.assertEqual = self.env.assertEqual
        self.assertRaises = self.env.assertRaises
        self.assertTrue = self.env.assertTrue
        self.assertAlmostEqual = self.env.assertAlmostEqual
        self.assertGreater = self.env.assertGreater
        self.restart_and_reload = self.env.restartAndReload
        self.assertResponseError = self.env.assertResponseError
        self.retry_with_rdb_reload = self.env.dumpAndReload
        self.assertNotEqual = self.env.assertNotEqual
        self.assertGreaterEqual = self.env.assertGreaterEqual

    def test_count(self):
        self.cmd('FLUSHALL')

        self.assertRaises(ResponseError, self.cmd, 'CF.RESERVE', 'cf')
        self.assertRaises(ResponseError, self.cmd, 'CF.RESERVE', 'cf', 'str')
        self.cmd('CF.RESERVE', 'cf', '1000')
        self.assertRaises(ResponseError, self.cmd, 'CF.RESERVE', 'cf', '1000')
        self.assertRaises(ResponseError, self.cmd, 'CF.RESERVE', 'tooSmall', '1')
        self.assertRaises(ResponseError, self.cmd, 'CF.INSERT', 'cf', 'CAPACITY', '-1', 'ITEMS', 'k0')
        self.assertRaises(ResponseError, self.cmd, 'CF.INSERT', 'cf', 'CAPACITY', '3', 'ITEMS', 'k0')
        self.assertRaises(ResponseError, self.cmd, 'CF.INSERTNX', 'cf', 'CAPACITY', '-1', 'ITEMS', 'k0')
        self.assertRaises(ResponseError, self.cmd, 'CF.INSERTNX', 'cf', 'CAPACITY', '3', 'ITEMS', 'k0')
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
        self.assertRaises(ResponseError, self.cmd, 'cf.del', 'cf')
        self.assertRaises(ResponseError, self.cmd, 'cf.del', 'bf', 'k1')

        for x in xrange(100):
            self.cmd('cf.add', 'nums', str(x))

        for x in xrange(100):
            self.assertEqual(1, self.cmd('cf.exists', 'nums', str(x)))

        # TODO: re-enable this portion after RLTest migration
        # for _ in self.retry_with_rdb_reload():
        #     for x in xrange(100):
        #         self.assertEqual(1, self.cmd('cf.exists', 'nums', str(x)))

    # TODO: re-enable this portion after RLTest migration
    # def test_aof(self):
    #     self.cmd('FLUSHALL')
    #     # Ensure we have a pretty small filter
    #     self.cmd('cf.reserve', 'smallCF', 4)
    #     for x in xrange(100):
    #         self.cmd('cf.add', 'smallCF', str(x))
    #     # Sanity check
    #     for x in xrange(100):
    #         self.assertEqual(1, self.cmd('cf.exists', 'smallCF', str(x)))

    #     self.restart_and_reload()
    #     for x in xrange(100):
    #         self.assertEqual(1, self.cmd('cf.exists', 'smallCF', str(x)))

    #     self.cmd('cf.reserve', 'smallCF2', 4, 'expansion', 2)
    #     for x in xrange(100):
    #         self.cmd('cf.add', 'smallCF2', str(x))
    #     # Sanity check
    #     for x in xrange(100):
    #         self.assertEqual(1, self.cmd('cf.exists', 'smallCF2', str(x)))

    # TODO: re-enable this portion after RLTest migration
    # self.restart_and_reload()
    # for x in xrange(100):
    #     self.assertEqual(1, self.cmd('cf.exists', 'smallCF2', str(x)))
    # self.assertEqual(580, self.cmd('MEMORY USAGE', 'smallCF'))
    # self.assertEqual(284, self.cmd('MEMORY USAGE', 'smallCF2'))

    def test_setnx(self):
        self.cmd('FLUSHALL')
        self.assertEqual(1, self.cmd('cf.addnx', 'cf', 'k1'))
        self.assertEqual(0, self.cmd('cf.addnx', 'cf', 'k1'))
        self.assertEqual(1, self.cmd('cf.count', 'cf', 'k1'))
        self.assertEqual(1, self.cmd('cf.add', 'cf', 'k1'))
        self.assertEqual(2, self.cmd('cf.count', 'cf', 'k1'))

    def test_insert(self):
        self.cmd('FLUSHALL')
        # Ensure insert with default capacity works
        self.assertEqual(1, self.cmd('cf.add', 'f1', 'foo'))
        self.assertRaises(ResponseError, self.cmd, 'cf.add', 'f1')
        self.assertEqual([1], self.cmd('cf.insert', 'f2', 'ITEMS', 'foo'))
        self.assertRaises(ResponseError, self.cmd, 'cf.insert', 'cf')
        d1 = self.cmd('cf.debug', 'f1')
        d2 = self.cmd('cf.debug', 'f2')
        self.assertRaises(ResponseError, self.cmd, 'cf.debug')
        self.assertRaises(ResponseError, self.cmd, 'cf.debug', 'noexist')
        self.assertTrue(d1)
        self.assertEqual(d1, d2)

        # Test NX
        self.assertEqual([0], self.cmd('cf.insertnx', 'f1', 'items', 'foo'))

        # Create a new filter with non-default capacity
        self.assertEqual([1], self.cmd('cf.insert', 'f3', 'CAPACITY', '10000', 'ITEMS', 'foo'))
        self.assertRaises(ResponseError, self.cmd, 'cf.insert', 'f3', 'NOCREATE', 'CAPACITY')
        self.assertRaises(ResponseError, self.cmd, 'cf.insert', 'f3', 'NOCREATE', 'CAPACITY', 'str')
        self.assertRaises(ResponseError, self.cmd, 'cf.insert', 'f3', 'NOCREATE', 'DONTEXIST')
        self.assertRaises(ResponseError, self.cmd, 'cf.insert', 'f3', 'NOCREATE', 'ITEMS')
        d3 = self.cmd('cf.debug', 'f3')
        self.assertEqual('bktsize:2 buckets:8192 items:1 deletes:0 filters:1 max_iterations:20 expansion:1', d3)
        self.assertNotEqual(d1, d3)

        # Test multi
        self.assertEqual([0, 1, 1], self.cmd('cf.insertnx', 'f3', 'ITEMS', 'foo', 'bar', 'baz'))

        # Test no auto creation
        with self.assertResponseError():
            self.cmd('cf.insert', 'f4', 'nocreate', 'items', 'foo')
        # Create it
        self.cmd('cf.insert', 'f4', 'items', 'foo')
        # Insert again to ensure our prior error was because of NOCREATE
        self.cmd('cf.insert', 'f4', 'nocreate', 'items', 'foo')

    def test_exists(self):
        self.cmd('FLUSHALL')
        self.assertEqual([1, 1, 1], self.cmd('CF.INSERT', 'f1', 'ITEMS', 'foo', 'bar', 'baz'))
        self.assertEqual([1, 1, 1], self.cmd('CF.MEXISTS', 'f1', 'foo', 'bar', 'baz'))

        # Test missing redis key
        self.assertEqual(0, self.cmd('CF.EXISTS', 'nonexist-key', 'blah'))
        self.assertEqual([0], self.cmd('CF.MEXISTS', 'nonexist-key', 'blah'))

        self.assertRaises(ResponseError, self.cmd, 'CF.MEXISTS', 'key')
        self.assertRaises(ResponseError, self.cmd, 'CF.MEXISTS')
        self.assertRaises(ResponseError, self.cmd, 'CF.EXISTS', 'key')
        self.assertRaises(ResponseError, self.cmd, 'CF.EXISTS')

    def test_mem_usage(self):
        self.cmd('FLUSHALL')
        self.cmd('CF.RESERVE', 'cf', '1000')
        if is_valgrind is False:
            self.assertEqual(1112, self.cmd('MEMORY USAGE', 'cf'))
        self.cmd('cf.insert', 'cf', 'nocreate', 'items', 'foo')
        if is_valgrind is False:
            self.assertEqual(1112, self.cmd('MEMORY USAGE', 'cf'))

    def test_max_iterations(self):
        self.cmd('FLUSHALL')
        self.cmd('CF.RESERVE a 10 MAXITERATIONS 10')
        d1 = self.cmd('cf.debug', 'a')
        self.assertEqual('bktsize:2 buckets:8 items:0 deletes:0 filters:1 max_iterations:10 expansion:1', d1)

        self.cmd('CF.RESERVE b 10')
        d2 = self.cmd('cf.debug', 'b')
        self.assertEqual('bktsize:2 buckets:8 items:0 deletes:0 filters:1 max_iterations:20 expansion:1', d2)

        self.assertRaises(ResponseError, self.cmd, 'CF.RESERVE a 10 MAXITERATIONS string')

    def test_num_deletes(self):
        self.cmd('FLUSHALL')
        self.cmd('cf.add', 'nums', 'Redis')
        self.cmd('cf.del', 'nums', 'Redis')
        d1 = self.cmd('cf.debug', 'nums')
        self.env.dumpAndReload()
        # for _ in self.client.retry_with_rdb_reload():   
        #     self.cmd('ping')    
        d2 = self.cmd('cf.debug', 'nums')
        self.assertEqual(d1, d2)

    def test_compact(self):
        self.env = Env()
        self.cmd('FLUSHALL')
        q = 100
        self.cmd('CF.RESERVE cf 8 MAXITERATIONS 50')

        for x in xrange(q):
            self.cmd('cf.add cf', str(x))

        for x in xrange(q):
            self.assertEqual(1, self.cmd('cf.exists cf', str(x)))

        str1 = self.cmd('cf.debug cf')[49:52]
        self.assertGreaterEqual(str1, "130")  # In experiments was larger than 130
        self.assertEqual(self.cmd('cf.compact cf'), 'OK')
        str2 = self.cmd('cf.debug cf')[49:52]
        self.assertGreaterEqual(str1, str2)  # Expect to see reduction after compaction

        self.assertRaises(ResponseError, self.cmd, 'CF.COMPACT a')
        self.assertRaises(ResponseError, self.cmd, 'CF.COMPACT a b')
        self.env = Env(decodeResponses=True)


    def test_compact(self):
        self.env = Env()
        self.cmd('FLUSHALL')
        q = 100
        self.cmd('CF.RESERVE cf 8 MAXITERATIONS 50')

        for x in xrange(q):
            self.cmd('cf.add cf', str(x))

        for x in xrange(q):
            self.assertEqual(1, self.cmd('cf.exists cf', str(x)))

        str1 = self.cmd('cf.debug cf')[49:52]
        self.assertGreaterEqual(str1, "130")  # In experiments was larger than 130
        self.assertEqual(self.cmd('cf.compact cf'), 'OK')
        str2 = self.cmd('cf.debug cf')[49:52]
        self.assertGreaterEqual(str1, str2)  # Expect to see reduction after compaction

        self.assertRaises(ResponseError, self.cmd, 'CF.COMPACT a')
        self.assertRaises(ResponseError, self.cmd, 'CF.COMPACT a b')
        self.env = Env(decodeResponses=True)


    def test_max_expansions(self):
        self.cmd('FLUSHALL')
        self.cmd('CF.RESERVE', 'cf', '4')
        for i in range(124):
            self.assertEqual(1, self.cmd('cf.add', 'cf', str(i)))
        self.assertRaises(ResponseError, self.cmd, 'cf.add', 'cf', str(2048))

    def test_bucket_size(self):
        self.cmd('FLUSHALL')
        self.cmd('CF.RESERVE a 64 BUCKETSIZE 1')
        self.cmd('CF.RESERVE b 64 BUCKETSIZE 2')
        self.cmd('CF.RESERVE c 256 BUCKETSIZE 4 MAXITERATIONS 500')
        for i in range(1000):
            self.cmd('CF.ADD a', str(i))
            self.cmd('CF.ADD b', str(i))
            self.cmd('CF.ADD c', str(i))

        for i in range(1000):
            self.assertEqual(self.cmd('CF.EXISTS a', str(i)), 1)
            self.assertEqual(self.cmd('CF.EXISTS b', str(i)), 1)
            self.assertEqual(self.cmd('CF.EXISTS c', str(i)), 1)

        self.assertEqual(self.cmd('CF.DEBUG a'),
                         'bktsize:1 buckets:64 items:1000 deletes:0 filters:18 max_iterations:20 expansion:1')
        self.assertEqual(self.cmd('CF.DEBUG b'),
                         'bktsize:2 buckets:32 items:1000 deletes:0 filters:17 max_iterations:20 expansion:1')
        self.assertEqual(self.cmd('CF.DEBUG c'),
                         'bktsize:4 buckets:64 items:1000 deletes:0 filters:4 max_iterations:500 expansion:1')

        self.assertRaises(ResponseError, self.cmd, 'CF.RESERVE err 10 BUCKETSIZE')
        self.assertRaises(ResponseError, self.cmd, 'CF.RESERVE err 10 BUCKETSIZE string')

    def test_expansion(self):
        self.cmd('FLUSHALL')
        self.cmd('CF.RESERVE a 64 EXPANSION 1')
        self.cmd('CF.RESERVE b 64 EXPANSION 2')
        self.cmd('CF.RESERVE c 64 EXPANSION 4 MAXITERATIONS 500')
        for i in range(1000):
            self.cmd('CF.ADD a', str(i))
            self.cmd('CF.ADD b', str(i))
            self.cmd('CF.ADD c', str(i))

        for i in range(1000):
            self.assertEqual(self.cmd('CF.EXISTS a', str(i)), 1)
            self.assertEqual(self.cmd('CF.EXISTS b', str(i)), 1)
            self.assertEqual(self.cmd('CF.EXISTS c', str(i)), 1)

        self.assertEqual(self.cmd('CF.DEBUG a'),
                         'bktsize:2 buckets:32 items:1000 deletes:0 filters:17 max_iterations:20 expansion:1')
        self.assertEqual(self.cmd('CF.DEBUG b'),
                         'bktsize:2 buckets:32 items:1000 deletes:0 filters:5 max_iterations:20 expansion:2')
        self.assertEqual(self.cmd('CF.DEBUG c'),
                         'bktsize:2 buckets:32 items:1000 deletes:0 filters:3 max_iterations:500 expansion:4')
        self.assertRaises(ResponseError, self.cmd, 'CF.RESERVE err 10 EXPANSION')
        self.assertRaises(ResponseError, self.cmd, 'CF.RESERVE err 10 EXPANSION string')

    def test_info(self):
        self.cmd('FLUSHALL')
        self.cmd('CF.RESERVE a 1000')
        self.assertEqual(self.cmd('CF.INFO a'), ['Size', 1080,
                                                 'Number of buckets', 512,
                                                 'Number of filters', 1,
                                                 'Number of items inserted', 0,
                                                 'Number of items deleted', 0,
                                                 'Bucket size', 2,
                                                 'Expansion rate', 1,
                                                 'Max iterations', 20])

        with self.assertResponseError():
            self.cmd('cf.info', 'bf')
        with self.assertResponseError():
            self.cmd('cf.info')

    def test_params(self):
        self.cmd('FLUSHALL')
        self.assertRaises(ResponseError, self.cmd, 'CF.RESERVE')
        self.assertRaises(ResponseError, self.cmd, 'CF.RESERVE err')
        self.cmd('CF.RESERVE err 1000')

        self.assertRaises(ResponseError, self.cmd, 'CF.ADD')
        self.assertRaises(ResponseError, self.cmd, 'CF.ADD err')
        self.assertRaises(ResponseError, self.cmd, 'CF.ADDNX')
        self.assertRaises(ResponseError, self.cmd, 'CF.ADDNX err')
        self.assertRaises(ResponseError, self.cmd, 'CF.INSERT')
        self.assertRaises(ResponseError, self.cmd, 'CF.INSERT err')
        self.assertRaises(ResponseError, self.cmd, 'CF.INSERT err element') # W/O ITEMS keyword
        self.assertRaises(ResponseError, self.cmd, 'CF.INSERTNX')
        self.assertRaises(ResponseError, self.cmd, 'CF.INSERTNX err')
        self.assertRaises(ResponseError, self.cmd, 'CF.INSERTNX err element') # W/O ITEMS keyword

        self.assertRaises(ResponseError, self.cmd, 'CF.DEL')
        self.assertRaises(ResponseError, self.cmd, 'CF.DEL err')
        self.assertRaises(ResponseError, self.cmd, 'CF.EXISTS')
        self.assertRaises(ResponseError, self.cmd, 'CF.EXISTS err')
        self.assertRaises(ResponseError, self.cmd, 'CF.MEXISTS')
        self.assertRaises(ResponseError, self.cmd, 'CF.MEXISTS err')
        self.assertRaises(ResponseError, self.cmd, 'CF.COUNT')
        self.assertRaises(ResponseError, self.cmd, 'CF.COUNT err')
        self.assertRaises(ResponseError, self.cmd, 'CF.INFO')

        self.assertRaises(ResponseError, self.cmd, 'CF.LOADCHUNK err')
        self.assertRaises(ResponseError, self.cmd, 'CF.LOADCHUNK err iterator') # missing data
        self.assertRaises(ResponseError, self.cmd, 'CF.SCANDUMP err')

class testCuckooNoCodec():
    def __init__(self):
        self.env = Env(decodeResponses=False)
        self.assertOk = self.env.assertTrue
        self.cmd = self.env.cmd
        self.assertEqual = self.env.assertEqual
        self.assertRaises = self.env.assertRaises
        self.assertTrue = self.env.assertTrue
        self.assertAlmostEqual = self.env.assertAlmostEqual
        self.assertGreater = self.env.assertGreater
        self.restart_and_reload = self.env.restartAndReload
        self.assertResponseError = self.env.assertResponseError
        self.retry_with_rdb_reload = self.env.dumpAndReload
        self.assertNotEqual = self.env.assertNotEqual
        self.assertGreaterEqual = self.env.assertGreaterEqual

    def test_scandump(self):
        self.cmd('FLUSHALL')
        maxrange = 500
        self.cmd('cf.reserve', 'cf', int(maxrange / 8))
        self.assertEqual([0, None], self.cmd('cf.scandump', 'cf', '0'))
        for x in xrange(maxrange):
            self.cmd('cf.add', 'cf', str(x))
        for x in xrange(maxrange):
            self.assertEqual(1, self.cmd('cf.exists', 'cf', str(x)))
        # Start with scandump
        self.assertRaises(ResponseError, self.cmd, 'cf.scandump', 'cf')
        self.assertRaises(ResponseError, self.cmd, 'cf.scandump', 'cf', 'str')
        self.assertRaises(ResponseError, self.cmd, 'cf.scandump', 'noexist', '0')
        chunks = []
        while True:
            last_pos = chunks[-1][0] if chunks else 0
            chunk = self.cmd('cf.scandump', 'cf', last_pos)
            if not chunk[0]:
                break
            chunks.append(chunk)
            # print("Scaning chunk... (P={}. Len={})".format(chunk[0], len(chunk[1])))
        self.cmd('del', 'cf')
        self.assertRaises(ResponseError, self.cmd, 'cf.loadchunk', 'cf')
        self.assertRaises(ResponseError, self.cmd, 'cf.loadchunk', 'cf', 'str')
        for chunk in chunks:
            print("Loading chunk... (P={}. Len={})".format(chunk[0], len(chunk[1])))
            self.cmd('cf.loadchunk', 'cf', *chunk)
        for x in xrange(maxrange):
            self.assertEqual(1, self.cmd('cf.exists', 'cf', str(x)))

    def test_scandump_with_expansion(self):
        self.cmd('FLUSHALL')
        maxrange = 500
    
        self.cmd('cf.reserve', 'cf', int(maxrange / 8), 'expansion', '2')
        self.assertEqual([0, None], self.cmd('cf.scandump', 'cf', '0'))
        for x in xrange(maxrange):
            self.cmd('cf.add', 'cf', str(x))
        for x in xrange(maxrange):
            self.assertEqual(1, self.cmd('cf.exists', 'cf', str(x)))

        chunks = []
        while True:
            i = 0
            last_pos = chunks[-1][0] if chunks else 0
            chunk = self.cmd('cf.scandump', 'cf', last_pos)
            if not chunk[0]:
                break
            chunks.append(chunk)
            print("Scaning chunk... (P={}. Len={})".format(chunk[0], len(chunk[1])))
        # delete filter
        self.cmd('del', 'cf')

        self.assertRaises(ResponseError, self.cmd, 'cf.loadchunk', 'cf')
        self.assertRaises(ResponseError, self.cmd, 'cf.loadchunk', 'cf', 'str')
        for chunk in chunks:
            print("Loading chunk... (P={}. Len={})".format(chunk[0], len(chunk[1])))
            self.cmd('cf.loadchunk', 'cf', *chunk)
        # check loaded filter
        for x in xrange(maxrange):
            self.assertEqual(1, self.cmd('cf.exists', 'cf', str(x)))
    
    def test_scandump_huge(self):
        self.cmd('FLUSHALL')
    
        self.cmd('cf.reserve', 'cf', 1024 * 1024 * 64)
        self.assertEqual([0, None], self.cmd('cf.scandump', 'cf', '0'))
        for x in xrange(6):
            self.cmd('cf.add', 'cf', 'foo')
        for x in xrange(6):
            self.assertEqual(1, self.cmd('cf.exists', 'cf', 'foo'))

        chunks = []
        while True:
            i = 0
            last_pos = chunks[-1][0] if chunks else 0
            chunk = self.cmd('cf.scandump', 'cf', last_pos)
            if not chunk[0]:
                break
            chunks.append(chunk)
            print("Scaning chunk... (P={}. Len={})".format(chunk[0], len(chunk[1])))
        # delete filter
        self.cmd('del', 'cf')

        self.assertRaises(ResponseError, self.cmd, 'cf.loadchunk', 'cf')
        self.assertRaises(ResponseError, self.cmd, 'cf.loadchunk', 'cf', 'str')
        for chunk in chunks:
            print("Loading chunk... (P={}. Len={})".format(chunk[0], len(chunk[1])))
            self.cmd('cf.loadchunk', 'cf', *chunk)
        # check loaded filter
        for x in xrange(6):
            self.assertEqual(1, self.cmd('cf.exists', 'cf', 'foo'))
