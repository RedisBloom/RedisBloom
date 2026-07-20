
from ast import arg
from common import *
import numpy as np
import redis
import math
import random
import struct
import zlib
from random import randint


def parse_tdigest_info(array_reply):
    reply_dict = {}
    for pos in range(0, len(array_reply), 2):
        property_name = array_reply[pos]
        property_value = array_reply[pos + 1]
        reply_dict[property_name] = property_value
    return reply_dict


# Build a TDIGEST.EXTERNALMERGE v1 ("TDB1") payload.
# Mirrors the layout documented in src/rm_tdigest.c. `centroids` is a list of
# (mean: float, weight: int) pairs, sorted ascending by mean.
def build_externalmerge(centroids, compression=100.0,
                    magic=b"TDB1", version=1,
                    override_num=None, override_crc=None):
    n = len(centroids) if override_num is None else override_num
    body = magic + bytes([version]) + struct.pack("<dI", compression, n)
    for m, _ in centroids:
        body += struct.pack("<d", m)
    for _, w in centroids:
        body += struct.pack("<q", w)
    crc = zlib.crc32(body) & 0xFFFFFFFF if override_crc is None else override_crc
    return body + struct.pack("<I", crc)


class testTDigest:
    def __init__(self):
        self.env = Env(decodeResponses=True)
        self.assertOk = self.env.assertTrue
        self.cmd = self.env.cmd
        self.assertEqual = self.env.assertEqual
        self.assertRaises = self.env.assertRaises
        self.assertTrue = self.env.assertTrue
        self.assertAlmostEqual = self.env.assertAlmostEqual
        self.assertGreater = self.env.assertGreater
        self.assertAlmostEqual = self.env.assertAlmostEqual
        self.restart_and_reload = self.env.restartAndReload

    def test_tdigest_create(self):
        self.cmd('FLUSHALL')
        for compression in range(100, 1000, 100):
            self.assertOk(self.cmd("tdigest.create", "tdigest", "compression", compression))
            self.assertEqual(
                compression,
                parse_tdigest_info(self.cmd("tdigest.info", "tdigest"))["Compression"],
            )
            self.assertOk(self.cmd("del", "tdigest"))
        self.assertOk(self.cmd("tdigest.create", "tdigest-default-compression"))
        self.assertEqual(
                100,
                parse_tdigest_info(self.cmd("tdigest.info", "tdigest-default-compression"))["Compression"],
            )

    def test_tdigest_create_twice(self):
        self.cmd('FLUSHALL')
        self.assertOk(self.cmd("tdigest.create", "tdigest"))
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.create", "tdigest")

    def test_negative_tdigest_create(self):
        self.cmd('FLUSHALL')
        self.cmd("SET", "tdigest", "B")
        # WRONGTYPE
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.create", "tdigest", "compression", 100
        )
        self.cmd("DEL", "tdigest")

        # arity upper
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.create", "tdigest", 100, 5,
        )
        # missing
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.create", "tdigest", "compression"
        )
        # parsing
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.create", "tdigest", "compression", "a"
        )
        # compression negative/zero value
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.create", "tdigest", "compression", 0
        )
        # compression negative/zero value
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.create", "tdigest", "compression", -1
        )
        # wrong keyword
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.create", "tdigest", "string", 100
        )
        self.cmd('FLUSHALL')
        # failed allocation
        self.assertRaises(
           redis.exceptions.ResponseError, self.cmd, "tdigest.create", "tdigest", "compression", '100000000000000000000'
        )

    def test_tdigest_create_compression_max_bound(self):
        # TD_MAX_COMPRESSION (src/rm_tdigest.c) bounds CREATE/MERGE's
        # COMPRESSION keyword the same way EXTERNALMERGE bounds its blob's
        # declared compression, so no single command can force an
        # arbitrarily large node-array allocation.
        self.cmd('FLUSHALL')
        max_centroids = 100000
        max_compression = (max_centroids - 10) // 6

        self.assertOk(self.cmd("tdigest.create", "atmax", "compression", max_compression))
        info = parse_tdigest_info(self.cmd("tdigest.info", "atmax"))
        self.assertEqual(max_compression, info["Compression"])
        self.assertEqual(max_centroids, info["Capacity"])

        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.create", "over",
            "compression", max_compression + 1
        )
        self.assertEqual(0, self.cmd("EXISTS", "over"))

        # Same bound applies to MERGE's explicit COMPRESSION keyword.
        self.assertOk(self.cmd("tdigest.create", "src", "compression", 100))
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.merge", "dst", 1, "src",
            "compression", max_compression + 1
        )
        self.assertEqual(0, self.cmd("EXISTS", "dst"))

    def test_tdigest_reset(self):
        self.cmd('FLUSHALL')
        self.assertOk(self.cmd("tdigest.create", "tdigest", "compression", 100))
        # reset on empty histogram
        self.assertOk(self.cmd("tdigest.reset", "tdigest"))
        # insert datapoints into sketch
        for x in range(100):
            self.assertOk(self.cmd("tdigest.add", "tdigest", random.random()))

        # assert we have 100 unmerged nodes
        self.assertEqual(
            100,
            parse_tdigest_info(self.cmd("tdigest.info", "tdigest"))["Unmerged nodes"],
        )

        self.assertOk(self.cmd("tdigest.reset", "tdigest"))

        # assert we have 100 unmerged nodes
        self.assertEqual(
            0, parse_tdigest_info(self.cmd("tdigest.info", "tdigest"))["Unmerged nodes"]
        )

    def test_negative_tdigest_reset(self):
        self.cmd('FLUSHALL')
        self.cmd("SET", "tdigest", "B")
        # WRONGTYPE
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.reset", "tdigest"
        )
        self.cmd("DEL", "tdigest")
        # empty key
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.reset", "tdigest"
        )

        self.assertOk(self.cmd("tdigest.create", "tdigest", "compression", 100))
        # arity lower
        self.assertRaises(redis.exceptions.ResponseError, self.cmd, "tdigest.reset")
        # arity upper
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.reset", "tdigest", 100
        )

    def test_tdigest_add(self):
        self.cmd('FLUSHALL')
        self.assertOk(self.cmd("tdigest.create", "tdigest", "compression", 100))
        # reset on empty histogram
        self.assertOk(self.cmd("tdigest.reset", "tdigest"))
        # insert datapoints into sketch
        for x in range(10000):
            self.assertOk(
                self.cmd(
                    "tdigest.add",
                    "tdigest",
                    random.random() * 10000,
                )
            )

        # check that multiple datapoints insertion behaves as expected
        self.assertOk(self.cmd("tdigest.create", "tdigest2"))
        args = ["tdigest.add", "tdigest2"]
        for x in range(100):
            args.append("1.0")
        self.assertOk(
                self.cmd(
                    " ".join(args)
                )
            )
        td_info = parse_tdigest_info(self.cmd("tdigest.info", "tdigest2"))
        total_weight = float(td_info["Merged weight"]) + float(
            td_info["Unmerged weight"]
        )
        self.assertEqual(100.0, total_weight)
        # total weight
        self.assertEqual(
                100.0,
                int(td_info["Observations"]),
            )

    def test_negative_tdigest_add(self):
        self.cmd('FLUSHALL')
        self.cmd("SET", "tdigest", "B")
        # WRONGTYPE
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.add", "tdigest", 100
        )
        self.cmd("DEL", "tdigest")
        self.assertOk(self.cmd("tdigest.create", "tdigest", "compression", 100))
        # arity lower
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.add", "tdigest"
        )
        # key does not exist
        self.assertRaises(
            ResponseError, self.cmd, "tdigest.add", "dont-exist", 100
        )
        # parsing
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.add", "tdigest", "a", 5, 10, 20
        )
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.add", "tdigest", 5.0, "a", 10, 20
        )
        # ensure nothing was added given  at least one input is not a valid floating-point value
        td_info = parse_tdigest_info(self.cmd("tdigest.info", "tdigest"))
        total_weight = float(td_info["Merged weight"]) + float(
            td_info["Unmerged weight"]
        )
        self.assertEqual(0, total_weight)

        # val parameter needs to be a finite number
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.add", "tdigest", "-inf",
        )
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.add", "tdigest", "+inf",
        )

    def test_tdigest_merge_to_empty(self):
        self.cmd("FLUSHALL")
        self.assertOk(self.cmd("tdigest.create", "to-tdigest{1}", "compression", 100))
        self.assertOk(self.cmd("tdigest.create", "from-tdigest{1}", "compression", 100))
        # insert datapoints into sketch
        for _ in range(100):
            self.assertOk(self.cmd("tdigest.add", "from-tdigest{1}", 1.0))
        # merge from-tdigest into to-tdigest
        self.assertOk(self.cmd("tdigest.merge", "to-tdigest{1}", 1 ,"from-tdigest{1}"))
        # assert we have same merged weight on both histograms ( given the to-histogram was empty )
        from_info = parse_tdigest_info(self.cmd("tdigest.info", "from-tdigest{1}"))
        total_weight_from = float(from_info["Merged weight"]) + float(
            from_info["Unmerged weight"]
        )
        to_info = parse_tdigest_info(self.cmd("tdigest.info", "to-tdigest{1}"))
        total_weight_to = float(to_info["Merged weight"]) + float(
            to_info["Unmerged weight"]
        )
        self.assertEqual(total_weight_from, total_weight_to)

    def test_tdigest_merge_itself(self):
        self.cmd("FLUSHALL")
        self.assertOk(self.cmd("tdigest.create", "to-tdigest", "compression", 100))
        # insert datapoints into sketch
        for _ in range(100):
            self.assertOk(self.cmd("tdigest.add", "to-tdigest", 1.0))
        # we should now have 100 weight on to-histogram
        to_info = parse_tdigest_info(self.cmd("tdigest.info", "to-tdigest"))
        total_weight_to = float(to_info["Merged weight"]) + float(
            to_info["Unmerged weight"]
        )
        self.assertEqual(100, total_weight_to)
        previous_weight = total_weight_to
        for iteration in range(5):
            self.assertOk(self.cmd("tdigest.merge", "to-tdigest", 1, "to-tdigest"))
            # we should now have ( iteration + 1 ) * 100 weight on to-histogram
            to_info = parse_tdigest_info(self.cmd("tdigest.info", "to-tdigest"))
            total_weight_to = float(to_info["Merged weight"]) + float(
                to_info["Unmerged weight"]
            )
            self.assertEqual(previous_weight * 2, total_weight_to)
            previous_weight = total_weight_to

    def test_tdigest_merge(self):
        self.cmd("FLUSHALL")
        to_tdigest = "to-tdigest{1}"
        from_tdigest = "from-tdigest{1}"
        self.assertOk(self.cmd("tdigest.create", to_tdigest, "compression", 100))
        self.assertOk(self.cmd("tdigest.create", from_tdigest, "compression", 100))
        # insert datapoints into sketch
        for _ in range(100):
            self.assertOk(self.cmd("tdigest.add", from_tdigest, 1.0))
        for _ in range(1000):
            self.assertOk(self.cmd("tdigest.add", to_tdigest, 1.0))
        # merge from-tdigest into to-tdigest
        self.assertOk(self.cmd("tdigest.merge", to_tdigest, 1, from_tdigest))
        # we should now have 1100 weight on to-histogram
        to_info = parse_tdigest_info(self.cmd("tdigest.info", to_tdigest))
        total_weight_to = float(to_info["Merged weight"]) + float(
            to_info["Unmerged weight"]
        )
        self.assertEqual(1100, total_weight_to)
        self.cmd("FLUSHALL")
        self.assertOk(self.cmd("tdigest.create", "to-1{1}", "compression", 55))
        self.assertOk(self.cmd("tdigest.create", "from-1{1}", "compression", 100))
        self.assertOk(self.cmd("tdigest.create", "from-2{1}", "compression", 200))
        self.assertOk(self.cmd("tdigest.create", "from-3{1}", "compression", 300))
        # insert datapoints into sketch
        self.assertOk(self.cmd("tdigest.add", "from-1{1}", 1.0))
        for _ in range(0,10):
            self.assertOk(self.cmd("tdigest.add", "from-2{1}", 1.0))
        # merge to a t-digest with max compression of all inputs which is 200
        self.assertOk(self.cmd("tdigest.merge", "to-tdigest-100{1}", "2", "from-1{1}", "from-2{1}"))
        to_info = parse_tdigest_info(self.cmd("tdigest.info", "to-tdigest-100{1}"))
        # ensure tha the destination t-digest has the largest compression of all input t-digests
        compression = int(to_info["Compression"])
        self.assertEqual(200, compression)
        # assert we have same merged weight on both histograms ( given the to-histogram was empty )
        total_weight_to = float(to_info["Merged weight"]) + float(
            to_info["Unmerged weight"]
        )
        total_weight_from = 10.0 + 1.0
        self.assertEqual(total_weight_from, total_weight_to)

        # merge to a t-digest that already exists so we will preserve its compression
        self.assertOk(self.cmd("tdigest.merge", "to-1{1}", "2", "from-1{1}", "from-2{1}"))
        to_info = parse_tdigest_info(self.cmd("tdigest.info", "to-1{1}"))
        # ensure tha the destination t-digest has the largest compression of all input t-digests
        compression = int(to_info["Compression"])
        self.assertEqual(55, compression)

        # merge to a t-digest with non-default compression
        self.assertOk(self.cmd("tdigest.merge", "to-tdigest-50{1}", "2","from-1{1}", "from-2{1}", "COMPRESSION", "50"))
        # ensure tha the destination t-digest has the passed compression
        to_info = parse_tdigest_info(self.cmd("tdigest.info", "to-tdigest-50{1}"))
        compression = int(to_info["Compression"])
        self.assertEqual(50, compression)

        # merge to a t-digest that already exists with non-default compression
        self.assertOk(self.cmd("tdigest.merge", "to-tdigest-50{1}", "2","from-1{1}", "from-2{1}", "COMPRESSION", "500"))
        # ensure tha the destination t-digest has the passed compression
        to_info = parse_tdigest_info(self.cmd("tdigest.info", "to-tdigest-50{1}"))
        compression = int(to_info["Compression"])
        self.assertEqual(500, compression)

        # merge to a t-digest that already exists but given we specify override it will use the max compression of all inputs
        self.assertOk(self.cmd("tdigest.merge", "to-tdigest-50{1}", "2","from-1{1}", "from-2{1}", "OVERRIDE"))
        # ensure tha the destination t-digest has the passed compression
        to_info = parse_tdigest_info(self.cmd("tdigest.info", "to-tdigest-50{1}"))
        compression = int(to_info["Compression"])
        self.assertEqual(200, compression)

    def test_tdigest_merge_percentile(self):
        self.cmd("FLUSHALL")
        self.assertOk(self.cmd("tdigest.create", "from-1{1}", "compression", 500))
        # insert datapoints into sketch
        for x in range(1, 10000):
            self.assertOk(self.cmd("tdigest.add", "from-1{1}", x * 0.01))
        # merge to a t-digest with default compression
        self.assertOk(self.cmd("tdigest.merge", "to-tdigest-500{1}", "1","from-1{1}", "COMPRESSION", "500"))
        # assert min min/max have same result as quantile 0 and 1
        self.assertEqual(
            float(self.cmd("tdigest.max", "to-tdigest-500{1}")),
            float(self.cmd("tdigest.quantile", "to-tdigest-500{1}", 1.0)[0]),
        )
        self.assertEqual(
            float(self.cmd("tdigest.min", "to-tdigest-500{1}")),
            float(self.cmd("tdigest.quantile", "to-tdigest-500{1}", 0.0)[0]),
        )
        self.assertAlmostEqual(
            1.0, float(self.cmd("tdigest.quantile", "to-tdigest-500{1}", 0.01)[0]), 0.01
        )
        self.assertAlmostEqual(
            99.0, float(self.cmd("tdigest.quantile", "to-tdigest-500{1}", 0.99)[0]), 0.01
        )
        self.assertAlmostEqual(
            99.0, float(self.cmd("tdigest.quantile", "to-tdigest-500{1}", 0.01, 0.99)[1]), 0.01
        )
        expected = [1,50,95,99]
        res = self.cmd("tdigest.quantile", "to-tdigest-500{1}", 0.01, 0.5, 0.95, 0.99)
        self.env.debugPrint(res)
        for i in range(len(res)):
            self.assertAlmostEqual(expected[i], float(res[i]), 0.01)

    def test_negative_tdigest_merge(self):
        self.cmd('FLUSHALL')
        self.cmd("SET", "to-tdigest", "B")
        self.cmd("SET", "from-tdigest", "B")
        self.assertOk(self.cmd("tdigest.create", "from-1"))

        # WRONGTYPE
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.merge", "to-tdigest", "1", "from-tdigest"
        )
        # WRONGTYPE in the one of the inputs
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.merge", "to-tdigest", "2", "from-1", "from-tdigest"
        )
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.merge", "to-tdigest", "2", "from-tdigest", "from-1"
        )
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.merge", "to-tdigest", "1", "from-tdigest", "COMPRESSION", "a"
        )
        self.cmd("DEL", "to-tdigest")
        # arity lower
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.merge"
        )
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.merge", "to-tdigest"
        )
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.merge", "to-tdigest", "1"
        )
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.merge", "to-tdigest", "1", "from-tdigest", "COMPRESSION"
        )
        # wrong keyword
        self.assertRaises(
            redis.exceptions.ResponseError,
            self.cmd,
            "tdigest.merge",
            "to-tdigest",
            "1",
            "from-tdigest",
            "extra-arg",
        )
        # arity upper
        self.assertRaises(
            redis.exceptions.ResponseError,
            self.cmd,
            "tdigest.merge",
            "to-tdigest",
            "1",
            "from-tdigest",
            "OVERRIDE",
            "extra-arg"
        )
        # numkeys needs to be a positive integer
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.merge", "to-tdigest", "-1", "from-tdigest"
        )
        # numkeys needs to be a positive integer
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.merge", "to-tdigest", "0", "from-tdigest"
        )
        # bad keyword
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.merge", "to-tdigest-500", "1","from-1",
                                                      "bad_keyword", "500"
        )
        # allocation of destination digest failed
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.merge", "to-tdigest", "1","from-1",
                                                      "COMPRESSION", "10000000000000000000"
        )

    def test_negative_tdigest_merge_crashes(self):
        # reported crash on merge to self where key does not exist
        self.cmd('FLUSHALL')
        for _ in range(1,1000):
            self.assertRaises(
                redis.exceptions.ResponseError, self.cmd,
                "tdigest.merge", "z", "5","z","z","z","z","z", "COMPRESSION", "3"
            )


    def test_negative_tdigest_merge_crashes_recursive(self):
        self.cmd('FLUSHALL')
        self.assertOk(self.cmd('tdigest.create x{1} COMPRESSION 1000'))
        self.assertOk(self.cmd('tdigest.create y{1} COMPRESSION 1000'))
        self.assertOk(self.cmd('tdigest.add x{1} 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20'))
        self.assertOk(self.cmd('tdigest.add y{1} 101 102 103 104 105 106 107 108 109 110 111 112 113 114 115 116 117 118 119 120'))
        try:
            for x in range(1,500):
                self.cmd('tdigest.merge z{1} 5 x{1} y{1} x{1} y{1} x{1}')
                self.cmd('tdigest.merge z{1} 5 z{1} z{1} z{1} z{1} z{1}')
        except redis.exceptions.ResponseError as e:
            error_str = e.__str__()
            self.assertTrue("overflow detected" in error_str)


    def test_tdigest_min_max(self):
        self.cmd('FLUSHALL')
        self.assertOk(self.cmd("tdigest.create", "tdigest"))
        # test for no datapoints first
        self.assertEqual('nan', self.cmd("tdigest.min", "tdigest"))
        self.assertEqual('nan', self.cmd("tdigest.max", "tdigest"))
        # insert datapoints into sketch
        for x in range(1, 101):
            self.assertOk(self.cmd("tdigest.add", "tdigest", x))
        # min/max
        self.assertEqual(100, float(self.cmd("tdigest.max", "tdigest")))
        self.assertEqual(1, float(self.cmd("tdigest.min", "tdigest")))

    def test_negative_tdigest_min_max(self):
        self.cmd('FLUSHALL')
        self.cmd("SET", "tdigest", "B")
        # WRONGTYPE
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.min", "tdigest"
        )
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.max", "tdigest"
        )
        # key does not exist
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.min", "dont-exist"
        )
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.max", "dont-exist"
        )

        self.cmd("DEL", "tdigest")
        self.assertOk(self.cmd("tdigest.create", "tdigest"))
        # arity lower
        self.assertRaises(redis.exceptions.ResponseError, self.cmd, "tdigest.min")
        self.assertRaises(redis.exceptions.ResponseError, self.cmd, "tdigest.max")
        # arity upper
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.min", "tdigest", 1
        )
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.max", "tdigest", 1
        )

    def test_tdigest_quantile(self):
        self.cmd('FLUSHALL')
        self.assertOk(self.cmd("tdigest.create", "tdigest", "compression", 500))
        # insert datapoints into sketch
        for x in range(1, 10000):
            self.assertOk(self.cmd("tdigest.add", "tdigest", x * 0.01))
        # assert min min/max have same result as quantile 0 and 1
        self.assertEqual(
            float(self.cmd("tdigest.max", "tdigest")),
            float(self.cmd("tdigest.quantile", "tdigest", 1.0)[0]),
        )
        self.assertEqual(
            float(self.cmd("tdigest.min", "tdigest")),
            float(self.cmd("tdigest.quantile", "tdigest", 0.0)[0]),
        )
        self.assertAlmostEqual(
            1.0, float(self.cmd("tdigest.quantile", "tdigest", 0.01)[0]), 0.01
        )
        self.assertAlmostEqual(
            99.0, float(self.cmd("tdigest.quantile", "tdigest", 0.99)[0]), 0.01
        )
        self.assertAlmostEqual(
            99.0, float(self.cmd("tdigest.quantile", "tdigest", 0.01, 0.99)[1]), 0.01
        )
        expected = [1.0,50.0,95.0,99.0]
        res = self.cmd("tdigest.quantile", "tdigest", 0.01, 0.5, 0.95, 0.99)
        for i in range(len(res)):
            self.assertAlmostEqual(
                expected[i], float(res[i]), 0.01
            )
        # the reply provides the output percentiles in ordered manner
        expected = [95.0,99.0,1.0,50.0]
        res = self.cmd("tdigest.quantile", "tdigest", 0.95, 0.99, 0.01, 0.5)
        for i in range(len(res)):
            self.assertAlmostEqual(
                expected[i], float(res[i]), 0.01
            )

    def test_negative_tdigest_quantile(self):
        self.cmd('FLUSHALL')
        self.cmd("SET", "tdigest", "B")
        # WRONGTYPE
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.quantile", "tdigest", 0.9
        )
        # key does not exist
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.quantile", "dont-exist", 0.9
        )
        self.cmd("DEL", "tdigest")
        self.assertOk(self.cmd("tdigest.create", "tdigest"))
        # arity lower
        self.assertRaises(redis.exceptions.ResponseError, self.cmd, "tdigest.quantile")
        # parsing
        self.assertRaises(
            redis.exceptions.ResponseError,
            self.cmd,
            "tdigest.quantile",
            "tdigest",
            1,
            "a",
        )
        # parsing quantile needs to be between [0,1]
        self.assertRaises(
            redis.exceptions.ResponseError,
            self.cmd,
            "tdigest.quantile",
            "tdigest",
            -0.5,
        )
        # parsing quantile needs to be between [0,1]
        self.assertRaises(
            redis.exceptions.ResponseError,
            self.cmd,
            "tdigest.quantile",
            "tdigest",
            1.1,
        )
        # parsing
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.quantile", "tdigest", "a"
        )

    def test_tdigest_cdf(self):
        self.cmd('FLUSHALL')
        self.assertOk(self.cmd("tdigest.create", "tdigest", "compression", 500))
        # insert datapoints into sketch
        for x in range(1, 100):
            self.assertOk(self.cmd("tdigest.add", "tdigest", x))

        self.assertAlmostEqual(
            0.01, float(self.cmd("tdigest.cdf", "tdigest", 1.0)[0]), 0.01
        )
        self.assertAlmostEqual(
            0.99, float(self.cmd("tdigest.cdf", "tdigest", 99.0)[0]), 0.01
        )
        self.assertAlmostEqual(
            0.99, float(self.cmd("tdigest.cdf", "tdigest", 1.0, 99.0)[1]), 0.01
        )
        self.assertAlmostEqual(
            0.01, float(self.cmd("tdigest.cdf", "tdigest", 99.0, 1.0)[1]), 0.01
        )

    def test_negative_tdigest_cdf(self):
        self.cmd('FLUSHALL')
        self.cmd("SET", "tdigest", "B")
        # WRONGTYPE
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.cdf", "tdigest", 0.9
        )
        # key does not exist
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.cdf", "dont-exist", 0.9
        )
        self.cmd("DEL", "tdigest")
        self.assertOk(self.cmd("tdigest.create", "tdigest"))
        # arity lower
        self.assertRaises(redis.exceptions.ResponseError, self.cmd, "tdigest.cdf")
        # parsing
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.cdf", "tdigest", "a"
        )
        # error with multi values
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.cdf", "tdigest", 1.0, 'foo'
        )

    def test_tdigest_rank(self):
        self.cmd('FLUSHALL')
        self.assertOk(self.cmd("tdigest.create", "tdigest", "compression", 500))
        # insert datapoints into sketch
        for x in range(0, 20):
            self.assertOk(self.cmd("tdigest.add", "tdigest", x))

        # -1 when value < value of the smallest observation
        self.assertEqual(-1, float(self.cmd("tdigest.rank", "tdigest", -1)[0]))
        # rank from cdf of min
        self.assertEqual(0, float(self.cmd("tdigest.rank", "tdigest", 0)[0]))
        # rank from cdf of max
        self.assertEqual(19, float(self.cmd("tdigest.rank", "tdigest", 19)[0]))
        # rank from cdf above max
        self.assertEqual(20, float(self.cmd("tdigest.rank", "tdigest", 20)[0]))
        # rank within [min,max]
        self.assertEqual(18, float(self.cmd("tdigest.rank", "tdigest", 18)[0]))
        self.assertEqual(10, float(self.cmd("tdigest.rank", "tdigest", 10)[0]))
        self.assertEqual(1, float(self.cmd("tdigest.rank", "tdigest", 1)[0]))
        # multiple inputs test
        self.assertEqual([-1,20,9], self.cmd("tdigest.rank", "tdigest", -20, 20, 9))

    def test_tdigest_revrank(self):
        self.cmd('FLUSHALL')
        self.assertOk(self.cmd("tdigest.create", "tdigest", "compression", 500))
        # insert datapoints into sketch
        for x in range(0, 20):
            self.assertOk(self.cmd("tdigest.add", "tdigest", x))

        # -1 when value > value of the largest observation
        self.assertEqual(-1, float(self.cmd("tdigest.revrank", "tdigest", 20)[0]))
        # rank from cdf of min
        self.assertEqual(19, float(self.cmd("tdigest.revrank", "tdigest", 0)[0]))
        # rank from cdf of max
        self.assertEqual(0, float(self.cmd("tdigest.revrank", "tdigest", 19)[0]))
        # rank from cdf above max
        self.assertEqual(-1, float(self.cmd("tdigest.revrank", "tdigest", 50)[0]))
        # rank within [min,max]
        self.assertEqual(1, float(self.cmd("tdigest.revrank", "tdigest", 18)[0]))
        self.assertEqual(9, float(self.cmd("tdigest.revrank", "tdigest", 10)[0]))
        self.assertEqual(18, float(self.cmd("tdigest.revrank", "tdigest", 1)[0]))
        # multiple inputs test
        self.assertEqual([-1,19,9], self.cmd("tdigest.revrank", "tdigest", 21, 0, 10))

    def test_tdigest_rank_and_revrank(self):
        self.cmd('FLUSHALL')
        self.assertOk(self.cmd("tdigest.create", "t", "compression","1000"))
        self.assertOk(self.cmd('TDIGEST.ADD', 't', '1', '2', '2', '3', '3', '3', '4', '4', '4', '4', '5', '5', '5', '5', '5'))
        self.assertEqual([-1, 0, 2, 4, 8, 12, 15], self.cmd('TDIGEST.RANK', 't', '0', '1', '2', '3', '4', '5', '6'))
        self.assertEqual([15, 14, 13, 10, 7, 2, -1], self.cmd('TDIGEST.REVRANK', 't', '0', '1', '2', '3', '4', '5', '6'))

        # RANK on an empty sketch
        self.assertOk(self.cmd("tdigest.create", "empty"))
        self.assertEqual([-2, -2], self.cmd('TDIGEST.RANK', 'empty', '0', '1'))

        # REVRANK on an empty sketch
        self.assertEqual([-2, -2], self.cmd('TDIGEST.REVRANK', 'empty', '0', '1'))

        # round down RANK
        self.assertOk(self.cmd("tdigest.create", "s", "compression","1000"))
        self.assertOk(self.cmd('TDIGEST.ADD', 's', '10', '20', '30', '40', '50', '60'))
        self.assertEqual([-1, 0, 1, 2, 3, 4, 5, 6], self.cmd('TDIGEST.RANK', 's', '0', '10', '20', '30', '40', '50', '60', '70'))


    def test_negative_tdigest_rank(self):
        self.cmd('FLUSHALL')
        self.cmd("SET", "tdigest", "B")
        # WRONGTYPE
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.rank", "tdigest", 0.9
        )
        # key does not exist
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.rank", "dont-exist", 0.9
        )
        self.cmd("DEL", "tdigest")
        self.assertOk(self.cmd("tdigest.create", "tdigest"))
        # arity lower
        self.assertRaises(redis.exceptions.ResponseError, self.cmd, "tdigest.rank")
        # parsing
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.rank", "tdigest", np.nan
        )
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.rank", "tdigest", "a", 0.9
        )
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.rank", "tdigest", 1.5, "a"
        )

    def test_tdigest_byrank(self):
        self.cmd('FLUSHALL')
        self.assertOk(self.cmd("tdigest.create", "tdigest", "compression", 500))
        # insert datapoints into sketch
        self.assertOk(self.cmd("tdigest.add tdigest 1 2 3 4 5 6 7 8 9 10"))

        # rank 0 is precise ( equal to minimum )
        self.assertEqual(1, float(self.cmd("tdigest.byrank", "tdigest", 0)[0]))
        # rank of N
        self.assertEqual("inf", self.cmd("tdigest.byrank", "tdigest", 10)[0])
        # rank larger than total count
        self.assertEqual("inf", self.cmd("tdigest.byrank", "tdigest", 100)[0])
        # inverse rank of N-1: [1,10]
        self.assertEqual(10, float(self.cmd("tdigest.byrank", "tdigest", 9)[0]))

    def test_tdigest_byrevrank(self):
        self.cmd('FLUSHALL')
        self.assertOk(self.cmd("tdigest.create", "tdigest", "compression", 1000))
        # insert datapoints into sketch
        for x in range(1, 11):
            self.assertOk(self.cmd("tdigest.add", "tdigest", x))

        # inverse rank 0
        self.assertEqual(10, float(self.cmd("tdigest.byrevrank", "tdigest", 0)[0]))
        # inverse rank of N
        self.assertEqual("-inf", self.cmd("tdigest.byrevrank", "tdigest", 10)[0])
        # inverse rank larger than total count
        self.assertEqual("-inf", self.cmd("tdigest.byrevrank", "tdigest", 100)[0])
        # inverse rank of N-1
        self.assertEqual(1.0, float(self.cmd("tdigest.byrevrank", "tdigest", 9)[0]))

        # reset the sketch
        self.assertOk(self.cmd("tdigest.reset", "tdigest"))
        self.assertOk(self.cmd("TDIGEST.ADD tdigest 1 2 2 3 3 3 4 4 4 4 5 5 5 5 5"))
        expected_revrank = ['5', '5', '5', '5', '5', '4', '4', '4', '4', '3', '3', '3', '2', '2', '1', '-inf']
        expected_rank = ['inf', '5', '5', '5', '5', '5', '4', '4', '4', '4', '3', '3', '3', '2', '2', '1']
        self.assertEqual(expected_revrank, self.cmd("TDIGEST.BYREVRANK tdigest 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15"))
        self.assertEqual(expected_rank, self.cmd("TDIGEST.BYRANK tdigest 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0"))
        self.assertEqual(expected_rank [1:], expected_revrank[:-1])

    def test_negative_tdigest_byrank(self):
        self.cmd('FLUSHALL')
        self.cmd("SET", "tdigest", "B")
        # WRONGTYPE
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.byrank", "tdigest", 0.9
        )
        # key does not exist
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.byrank", "dont-exist", 0.9
        )
        self.cmd("DEL", "tdigest")
        self.assertOk(self.cmd("tdigest.create", "tdigest"))
        # arity lower
        self.assertRaises(redis.exceptions.ResponseError, self.cmd, "tdigest.byrank")
        # Error if rank is negative
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.byrank", "tdigest", -1
        )
        # Error if rank is not an integer
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.byrank", "tdigest", 0.5
        )
        # parsing
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.byrank", "tdigest", np.nan
        )
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.byrank", "tdigest", "a", 0.9
        )
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.byrank", "tdigest", 1.5, "a"
        )

    def test_tdigest_trimmed_mean(self):
        self.cmd('FLUSHALL')
        self.assertOk(self.cmd("tdigest.create", "tdigest", "compression", 500))
        # insert datapoints into sketch
        for x in range(0, 20):
            self.assertOk(self.cmd("tdigest.add", "tdigest", x))

        self.assertAlmostEqual(
            9.5, float(self.cmd("tdigest.trimmed_mean", "tdigest", 0.1,0.9)), 0.01
        )
        self.assertAlmostEqual(
            9.5, float(self.cmd("tdigest.trimmed_mean", "tdigest", 0.0,1.0)), 0.01
        )
        self.assertAlmostEqual(
            9.5, float(self.cmd("tdigest.trimmed_mean", "tdigest", 0.2,0.8)), 0.01
        )
        self.assertOk(self.cmd("tdigest.reset", "tdigest"))
        self.assertEqual(
            "nan", self.cmd("tdigest.trimmed_mean", "tdigest", 0.2,0.8)
        )
        # insert datapoints into sketch
        # given a high number of datapoints, the trimmed mean between a range on those datapoints
        # is approximate to the precise mean of the interval range
        for x in range(1, 10001):
            self.assertOk(self.cmd("tdigest.add", "tdigest", float(x)/1000.0))
        for x in range(1, 10):
            low_cut = float(x)/10.0
            high_cut = low_cut + 0.1
            self.assertAlmostEqual(
                x+0.5, float(self.cmd("tdigest.trimmed_mean", "tdigest", low_cut, high_cut)), 0.01
            )
        # simple confirmation that the when having:
        #   9 observations of value 1
        #   1 observation of value 5
        # and comparing vs sheets
        #      TRIMMEAN(G2:G11,0.2) we get 1.0
        #      TRIMMEAN(G2:G11,0.19) we get 1.4
        #      TRIMMEAN(G2:G11,0.10) we get 1.4
        #      TRIMMEAN(G2:G11,0.02) we get 1.4
        # if we replicate this on our trimmed_mean implementation we get the same results
        self.assertOk(self.cmd("tdigest.reset", "tdigest"))
        for x in range(0,9):
            self.assertOk(self.cmd("tdigest.add", "tdigest", 1.0))
        self.assertOk(self.cmd("tdigest.add", "tdigest", 5.0))
        self.assertAlmostEqual(
                1.0, float(self.cmd("tdigest.trimmed_mean", "tdigest", 0.10, 0.90)), 0.01
            )
        self.assertAlmostEqual(
                1.4, float(self.cmd("tdigest.trimmed_mean", "tdigest", 0.095, 0.905)), 0.01
            )
        self.assertAlmostEqual(
                1.4, float(self.cmd("tdigest.trimmed_mean", "tdigest", 0.05, 0.95)), 0.01
            )
        self.assertAlmostEqual(
                1.4, float(self.cmd("tdigest.trimmed_mean", "tdigest", 0.01, 0.99)), 0.01
            )

    def test_negative_tdigest_trimmed_mean(self):
        self.cmd('FLUSHALL')
        self.cmd("SET", "tdigest", "B")
        # WRONGTYPE
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.trimmed_mean", "tdigest", 0.9
        )
        # key does not exist
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.trimmed_mean", "dont-exist", 0.9
        )
        self.cmd("DEL", "tdigest")
        self.assertOk(self.cmd("tdigest.create", "tdigest"))
        # arity lower
        self.assertRaises(redis.exceptions.ResponseError, self.cmd, "tdigest.trimmed_mean")
        # arity upper
        self.assertRaises(
            redis.exceptions.ResponseError,
            self.cmd,
            "tdigest.trimmed_mean",
            "tdigest",
            1,
            1,
            1,
        )
        # parsing
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.trimmed_mean", "tdigest", "a", "a"
        )
        # low_cut_percentile and high_cut_percentile should be in [0,1]
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.trimmed_mean", "tdigest", "10.0", "20.0"
        )
        # low_cut_percentile should be lower than high_cut_percentile
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.trimmed_mean", "tdigest", "0.9", "0.1"
        )
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.trimmed_mean", "tdigest", "0.1", "0.1"
        )

    def test_tdigest_info(self):
        self.cmd('FLUSHALL')
        self.assertOk(self.cmd("tdigest.create", "tdigest"))
        # insert datapoints into sketch
        for x in range(1, 101):
            self.assertOk(self.cmd("tdigest.add", "tdigest", x))
        td_info = parse_tdigest_info(self.cmd("tdigest.info", "tdigest"))
        # total weight
        self.assertEqual(
                100,
                int(td_info["Observations"]),
            )
        init_mem_usage = td_info["Memory usage"]
        # memory usage check
        self.assertTrue(
                init_mem_usage <= self.cmd("MEMORY", "USAGE", "tdigest"),
            )
        # independent of the datapoints this sketch has an invariant size after creation
        for x in range(1, 10001):
            self.assertOk(self.cmd("tdigest.add", "tdigest", x))
        td_info = parse_tdigest_info(self.cmd("tdigest.info", "tdigest"))
        self.assertEqual(
                init_mem_usage,
                td_info["Memory usage"],
            )
        previous_mem_usage = 0
        for compression in [100,200,300,400,500]:
            self.cmd('FLUSHALL')
            self.assertOk(self.cmd("tdigest.create", "tdigest", "compression", compression))
            td_info = parse_tdigest_info(self.cmd("tdigest.info", "tdigest"))
            current_mem_usage = td_info["Memory usage"]
            self.assertTrue(
                previous_mem_usage < current_mem_usage,
            )
            previous_mem_usage = current_mem_usage

    def test_negative_tdigest_info(self):
        self.cmd('FLUSHALL')
        self.cmd("SET", "tdigest", "B")
        # WRONGTYPE
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.info", "tdigest"
        )
        # dont exist
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.info", "dont-exist"
        )
        self.cmd("DEL", "tdigest")
        self.assertOk(self.cmd("tdigest.create", "tdigest"))
        # arity lower
        self.assertRaises(redis.exceptions.ResponseError, self.cmd, "tdigest.info")
        # arity upper
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.info", "tdigest", 1
        )

    def test_save_load(self):
        self.cmd('FLUSHALL')
        self.assertOk(self.cmd("tdigest.create", "tdigest"))
        # insert datapoints into sketch
        for _ in range(1, 101):
            self.assertOk(self.cmd("tdigest.add", "tdigest", 1.0))
        self.assertEqual(True, self.cmd("SAVE"))
        mem_usage_prior_restart = self.cmd("MEMORY", "USAGE", "tdigest")
        tdigest_min = self.cmd("tdigest.min", "tdigest")
        tdigest_max = self.cmd("tdigest.max", "tdigest")
        self.restart_and_reload()
        # assert we have 100 unmerged nodes
        self.assertEqual(1, self.cmd("EXISTS", "tdigest"))
        self.assertEqual(
            100,
            float(
                parse_tdigest_info(self.cmd("tdigest.info", "tdigest"))["Merged weight"]
            ),
        )
        mem_usage_after_restart = self.cmd("MEMORY", "USAGE", "tdigest")
        self.assertEqual(mem_usage_prior_restart, mem_usage_after_restart)
        self.assertEqual(tdigest_min, self.cmd("tdigest.min", "tdigest"))
        self.assertEqual(tdigest_max, self.cmd("tdigest.max", "tdigest"))

    def test_tdigest_externalmerge_happy_path(self):
        self.cmd("FLUSHALL")
        # Sorted, finite means; positive int64 weights.
        centroids = [(1.0, 5), (2.5, 10), (3.7, 2), (10.0, 1)]
        blob = build_externalmerge(centroids, compression=100.0)
        # Create-if-missing: target key does not exist yet.
        self.assertOk(self.cmd("tdigest.externalmerge", "dst", blob))
        info = parse_tdigest_info(self.cmd("tdigest.info", "dst"))
        total_weight = float(info["Merged weight"]) + float(info["Unmerged weight"])
        self.assertEqual(sum(w for _, w in centroids), total_weight)

        # A second merge accumulates rather than replaces.
        self.assertOk(self.cmd("tdigest.externalmerge", "dst", blob))
        info = parse_tdigest_info(self.cmd("tdigest.info", "dst"))
        total_weight2 = float(info["Merged weight"]) + float(info["Unmerged weight"])
        self.assertEqual(2 * sum(w for _, w in centroids), total_weight2)

    def test_tdigest_externalmerge_compression_on_create_vs_existing(self):
        # Creating a new key: the blob's declared compression governs, so a
        # high-fidelity client-side digest isn't silently downsampled to the
        # module default on its first flush.
        self.cmd("FLUSHALL")
        blob = build_externalmerge([(1.0, 1), (2.0, 1)], compression=250.0)
        self.assertOk(self.cmd("tdigest.externalmerge", "dst", blob))
        info = parse_tdigest_info(self.cmd("tdigest.info", "dst"))
        self.assertEqual(250, info["Compression"])

        # Merging into an existing key: the key's own compression governs,
        # and the blob's compression field is ignored (same rule as MERGE).
        self.assertOk(self.cmd("tdigest.create", "existing", "compression", 100))
        blob2 = build_externalmerge([(1.0, 1), (2.0, 1)], compression=999.0)
        self.assertOk(self.cmd("tdigest.externalmerge", "existing", blob2))
        info2 = parse_tdigest_info(self.cmd("tdigest.info", "existing"))
        self.assertEqual(100, info2["Compression"])

    def test_tdigest_externalmerge_max_compression_bound(self):
        # EXTERNALMERGE_MAX_COMPRESSION is derived so that the resulting
        # digest's internal capacity (6*compression+10) never exceeds
        # EXTERNALMERGE_MAX_CENTROIDS, matching the bound already applied to
        # the blob's own centroid count.
        self.cmd("FLUSHALL")
        # Mirrors TD_MAX_CENTROIDS / TD_MAX_COMPRESSION in src/rm_tdigest.c,
        # shared by TDIGEST.CREATE, TDIGEST.MERGE, and TDIGEST.EXTERNALMERGE.
        max_centroids = 100000
        max_compression = (max_centroids - 10) // 6

        # Exactly at the cap: accepted, and the resulting capacity matches.
        at_max = build_externalmerge([(1.0, 1), (2.0, 1)], compression=float(max_compression))
        self.assertOk(self.cmd("tdigest.externalmerge", "atmax", at_max))
        info = parse_tdigest_info(self.cmd("tdigest.info", "atmax"))
        self.assertEqual(max_compression, info["Compression"])
        self.assertEqual(max_centroids, info["Capacity"])

        # One over the cap: rejected, no key created.
        over = build_externalmerge([(1.0, 1), (2.0, 1)], compression=float(max_compression + 1))
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.externalmerge", "over", over
        )
        self.assertEqual(0, self.cmd("EXISTS", "over"))

        # A tiny blob declaring an absurd compression must not be able to
        # force a huge allocation for a brand-new key.
        huge = build_externalmerge([(1.0, 1), (2.0, 1)], compression=1e8)
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.externalmerge", "huge", huge
        )
        self.assertEqual(0, self.cmd("EXISTS", "huge"))

    def test_tdigest_externalmerge_overflow_leaves_no_partial_state(self):
        # Two centroids whose weights overflow the running int64 weight
        # accumulator on the second `td_add`. A rejected blob must fail
        # atomically: no partially-merged digest, and no orphaned key.
        self.cmd("FLUSHALL")
        int64_max = 2**63 - 1
        bad = build_externalmerge([(1.0, int64_max), (2.0, 1)])

        # Destination key does not exist yet: it must not be created either.
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.externalmerge", "fresh", bad
        )
        self.assertEqual(0, self.cmd("EXISTS", "fresh"))

        # Destination key already has data: a failed merge must not mutate it.
        good = build_externalmerge([(5.0, 3), (6.0, 4)], compression=100.0)
        self.assertOk(self.cmd("tdigest.externalmerge", "existing", good))
        info_before = parse_tdigest_info(self.cmd("tdigest.info", "existing"))
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.externalmerge", "existing", bad
        )
        info_after = parse_tdigest_info(self.cmd("tdigest.info", "existing"))
        self.assertEqual(info_before, info_after)

    def test_tdigest_externalmerge_equivalence_to_add(self):
        # Building a digest from centroids via EXTERNALMERGE should give the same
        # quantile estimates as inserting weighted samples via TDIGEST.ADD.
        self.cmd("FLUSHALL")
        centroids = [(float(x), 100) for x in range(1, 101)]
        blob = build_externalmerge(centroids, compression=100.0)
        self.assertOk(self.cmd("tdigest.create", "via_add", "compression", 100))
        for m, w in centroids:
            # ADD has no weight knob, so simulate by adding w copies.
            for _ in range(w):
                self.cmd("tdigest.add", "via_add", m)
        self.assertOk(self.cmd("tdigest.externalmerge", "via_blob", blob))
        for q in (0.1, 0.5, 0.9):
            a = float(self.cmd("tdigest.quantile", "via_add", q)[0])
            b = float(self.cmd("tdigest.quantile", "via_blob", q)[0])
            self.assertAlmostEqual(a, b, 1.0)

    def test_tdigest_externalmerge_wrongtype(self):
        self.cmd("FLUSHALL")
        self.cmd("SET", "strkey", "B")
        blob = build_externalmerge([(1.0, 1)])
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.externalmerge", "strkey", blob
        )

    def test_tdigest_externalmerge_arity(self):
        self.cmd("FLUSHALL")
        self.assertRaises(redis.exceptions.ResponseError, self.cmd, "tdigest.externalmerge")
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.externalmerge", "k"
        )

    def test_tdigest_externalmerge_rejects_malformed(self):
        # Every rejection branch in _ExternalMerge_Parse, exercised individually.
        self.cmd("FLUSHALL")
        good = [(1.0, 1), (2.0, 2)]

        # Too short.
        self.env.expect("tdigest.externalmerge", "k", b"\x00" * 8).error()

        # Bad magic.
        bad = build_externalmerge(good, magic=b"XXXX")
        self.env.expect("tdigest.externalmerge", "k", bad).error()

        # Bad version.
        bad = build_externalmerge(good, version=99)
        self.env.expect("tdigest.externalmerge", "k", bad).error()

        # num_centroids field claims more than the blob actually contains.
        bad = build_externalmerge(good, override_num=len(good) + 7)
        self.env.expect("tdigest.externalmerge", "k", bad).error()

        # num_centroids over the cap (we only need the field set; total length
        # is checked before the cap so build a header-only blob that claims a
        # huge N — but the cap-check runs first, so length doesn't matter here).
        header = b"TDB1" + bytes([1]) + struct.pack("<dI", 100.0, (1 << 21))
        bad = header + b"\x00" * 4
        self.env.expect("tdigest.externalmerge", "k", bad).error()

        # Invalid compression (NaN).
        bad = build_externalmerge(good, compression=float("nan"))
        self.env.expect("tdigest.externalmerge", "k", bad).error()

        # Invalid compression (non-positive).
        bad = build_externalmerge(good, compression=-1.0)
        self.env.expect("tdigest.externalmerge", "k", bad).error()

        # Compression too large: a tiny blob must not be able to force a huge
        # allocation for a brand-new key via an absurd declared compression.
        bad = build_externalmerge(good, compression=100000000.0)
        self.env.expect("tdigest.externalmerge", "k", bad).error()

        # Non-finite mean.
        bad = build_externalmerge([(float("inf"), 1)])
        self.env.expect("tdigest.externalmerge", "k", bad).error()

        # Non-positive weight.
        bad = build_externalmerge([(1.0, 0)])
        self.env.expect("tdigest.externalmerge", "k", bad).error()

        # Unsorted means.
        bad = build_externalmerge([(2.0, 1), (1.0, 1)])
        self.env.expect("tdigest.externalmerge", "k", bad).error()

        # CRC mismatch.
        bad = build_externalmerge(good, override_crc=0xDEADBEEF)
        self.env.expect("tdigest.externalmerge", "k", bad).error()

        # None of the rejections should have left a key behind.
        self.assertEqual(0, self.cmd("EXISTS", "k"))

    def test_tdigest_externalmerge_persistence(self):
        # EXTERNALMERGE-built digest should survive SAVE/restart like any other.
        self.cmd("FLUSHALL")
        blob = build_externalmerge([(float(x), 1) for x in range(1, 51)], compression=100.0)
        self.assertOk(self.cmd("tdigest.externalmerge", "dst", blob))
        weight_before = float(
            parse_tdigest_info(self.cmd("tdigest.info", "dst"))["Merged weight"]
        ) + float(
            parse_tdigest_info(self.cmd("tdigest.info", "dst"))["Unmerged weight"]
        )
        self.assertEqual(True, self.cmd("SAVE"))
        self.restart_and_reload()
        self.assertEqual(1, self.cmd("EXISTS", "dst"))
        weight_after = float(
            parse_tdigest_info(self.cmd("tdigest.info", "dst"))["Merged weight"]
        ) + float(
            parse_tdigest_info(self.cmd("tdigest.info", "dst"))["Unmerged weight"]
        )
        self.assertEqual(weight_before, weight_after)

    def test_insufficient_memory(self):
        if os.environ.get('SANITIZER') != None:
            self.env.skip()
        self.cmd("FLUSHALL")
        # Absurdly large compression is now rejected proactively (bounded by
        # TD_MAX_COMPRESSION) rather than relying on the allocator to fail.
        self.env.expect('tdigest.create', 'k', 'compression', 100000000000000000).error().contains('exceeds maximum allowed')

    def test_rdb_load_oob_guard(self):
        self.cmd('FLUSHALL')
        rdb_payload = b'\x07\x81L2\x12\xf96\x0f\x10\x00\x04\x00\x00\x00\x00\x00\x00(@\x04\x00\x00\x00\x00\x00\x00\xf0?\x04\x00\x00\x00\x00\x00\x00\xf0?\x02\x01\x02@a\x02\x01\x02\x01\x04\x00\x00\x00\x00\x00\x00\xf0?\x04\x00\x00\x00\x00\x00\x00\xf0?\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04CCCCCCCC\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x04AAAAAAAA\x00\xff\x0c\x008\x9c\x969\x91:\xcc5'
        self.env.expect('RESTORE', "key", 0, rdb_payload, 'REPLACE').error().contains('Bad data format')

    def test_rdb_load_cap_compression_consistency(self):
        # TDigestRdbLoad used to trust `cap`/`merged_nodes`/`unmerged_nodes` from
        # the wire as-is, decoupled from the buffer sizes td_new(compression)
        # actually allocates. A crafted RESTORE payload declaring a small
        # compression (small real buffer) but a large `cap` field passed the old
        # bounds check (merged_nodes <= cap) while writing far past the real
        # allocation on subsequent ADDs -- a heap buffer overflow that crashed
        # the server. Loading must now derive cap from compression and reject
        # any payload where the two disagree, or where compression itself is
        # out of bounds.
        self.cmd('FLUSHALL')

        def crc64_jones(data):
            poly = 0xad93d23594c935a9
            rpoly = int(f'{poly:064b}'[::-1], 2)
            crc = 0
            for byte in data:
                crc ^= byte
                for _ in range(8):
                    crc = (crc >> 1) ^ rpoly if crc & 1 else crc >> 1
            return crc & 0xFFFFFFFFFFFFFFFF

        def tamper(dump_bytes, cap_offset=None, fake_cap=None, compression_offset=None,
                   fake_compression=None):
            buf = bytearray(dump_bytes)
            if fake_cap is not None:
                buf[cap_offset] = 0x40 | (fake_cap >> 8)
                buf[cap_offset + 1] = fake_cap & 0xFF
            if fake_compression is not None:
                buf[compression_offset:compression_offset + 8] = struct.pack('<d', fake_compression)
            body = bytes(buf[:-8])
            buf[-8:] = struct.pack('<Q', crc64_jones(body))
            return bytes(buf)

        # Layout for a freshly-created (empty) tdigest DUMP: 10-byte module
        # header, opcode+8-byte-double compression at [10:19], min/max doubles,
        # then opcode+14bit-length cap at [37:40].
        self.assertOk(self.cmd('tdigest.create', 'src', 'compression', 10))
        dump = self.cmd('DUMP', 'src')

        # Sanity: the offsets above actually point at compression/cap on this
        # build's RDB encoding, not unrelated bytes.
        (orig_compression,) = struct.unpack('<d', dump[11:19])
        self.assertEqual(10.0, orig_compression)
        self.assertEqual(70, dump[39])  # cap = 6*10+10 = 70, fits in the low byte

        # cap inflated far past the real 70-element buffer.
        evil_cap = tamper(dump, cap_offset=38, fake_cap=10000)
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, 'RESTORE', 'evil_cap', 0, evil_cap
        )

        # compression beyond TD_MAX_COMPRESSION.
        evil_compression = tamper(dump, compression_offset=11, fake_compression=1e9)
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, 'RESTORE', 'evil_compression', 0,
            evil_compression
        )

        # negative compression.
        evil_negative = tamper(dump, compression_offset=11, fake_compression=-5.0)
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, 'RESTORE', 'evil_negative', 0, evil_negative
        )

        # Server must still be alive and the untampered key usable.
        self.assertOk(self.cmd('tdigest.add', 'src', 1.0))
        self.assertEqual(0, self.cmd('EXISTS', 'evil_cap'))
        self.assertEqual(0, self.cmd('EXISTS', 'evil_compression'))
        self.assertEqual(0, self.cmd('EXISTS', 'evil_negative'))

        # An untampered DUMP must still RESTORE correctly.
        self.cmd('DEL', 'src2')
        clean_dump = tamper(dump)  # re-checksum only, no field changes
        self.assertOk(self.cmd('RESTORE', 'src2', 0, clean_dump))
        info = parse_tdigest_info(self.cmd('tdigest.info', 'src2'))
        self.assertEqual(10, info['Compression'])
        self.assertEqual(70, info['Capacity'])
