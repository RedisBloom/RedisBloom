#!/usr/bin/env python3
import os
from random import randint
from RLTest import Env
from redis import ResponseError
import redis
import sys
import random
import math

is_valgrind = True if ("VGD" in os.environ or "VALGRIND" in os.environ) else False


def parse_tdigest_info(array_reply):
    reply_dict = {}
    for pos in range(0, len(array_reply), 2):
        property_name = array_reply[pos]
        property_value = array_reply[pos + 1]
        reply_dict[property_name] = property_value
    return reply_dict


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
        for compression in range(100, 1000, 100):
            self.assertOk(self.cmd("tdigest.create", "tdigest", compression))
            self.assertEqual(
                compression,
                parse_tdigest_info(self.cmd("tdigest.info", "tdigest"))["Compression"],
            )
        self.assertOk(self.cmd("tdigest.create", "tdigest-default-compression"))
        self.assertEqual(
                100,
                parse_tdigest_info(self.cmd("tdigest.info", "tdigest-default-compression"))["Compression"],
            )

    def test_negative_tdigest_create(self):
        self.cmd("SET", "tdigest", "B")
        # WRONGTYPE
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.create", "tdigest", 100
        )
        self.cmd("DEL", "tdigest")
        # arity upper
        self.assertRaises(
            redis.exceptions.ResponseError,
            self.cmd,
            "tdigest.create",
            "tdigest",
            100,
            5,
        )
        # parsing
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.create", "tdigest", "a"
        )
        # compression negative/zero value
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.create", "tdigest", 0
        )
        # compression negative/zero value
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.create", "tdigest", -1
        )

    def test_tdigest_reset(self):
        self.assertOk(self.cmd("tdigest.create", "tdigest", 100))
        # reset on empty histogram
        self.assertOk(self.cmd("tdigest.reset", "tdigest"))
        # insert datapoints into sketch
        for x in range(100):
            self.assertOk(self.cmd("tdigest.add", "tdigest", random.random(), 1.0))

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

        self.assertOk(self.cmd("tdigest.create", "tdigest", 100))
        # arity lower
        self.assertRaises(redis.exceptions.ResponseError, self.cmd, "tdigest.reset")
        # arity upper
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.reset", "tdigest", 100
        )

    def test_tdigest_add(self):
        self.assertOk(self.cmd("tdigest.create", "tdigest", 100))
        # reset on empty histogram
        self.assertOk(self.cmd("tdigest.reset", "tdigest"))
        # insert datapoints into sketch
        for x in range(10000):
            self.assertOk(
                self.cmd(
                    "tdigest.add",
                    "tdigest",
                    random.random() * 10000,
                    random.random() * 500 + 1.0,
                )
            )

    def test_negative_tdigest_add(self):
        self.cmd("SET", "tdigest", "B")
        # WRONGTYPE
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.add", "tdigest", 100, 100
        )
        self.cmd("DEL", "tdigest")
        self.assertOk(self.cmd("tdigest.create", "tdigest", 100))
        # arity lower
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.add", "tdigest"
        )
        # arity upper
        self.assertRaises(
            ResponseError, self.cmd, "tdigest.add", "tdigest", 100, 5, 100.0
        )
        # key does not exist
        self.assertRaises(
            ResponseError, self.cmd, "tdigest.add", "dont-exist", 100, 100
        )
        # parsing
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.add", "tdigest", "a", 5
        )
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.add", "tdigest", 5.0, "a"
        )

    def test_tdigest_merge(self):
        self.cmd("FLUSHALL")
        self.assertOk(self.cmd("tdigest.create", "to-tdigest", 100))
        self.assertOk(self.cmd("tdigest.create", "from-tdigest", 100))
        # insert datapoints into sketch
        for _ in range(100):
            self.assertOk(self.cmd("tdigest.add", "from-tdigest", 1.0, 1.0))
        for _ in range(100):
            self.assertOk(self.cmd("tdigest.add", "to-tdigest", 1.0, 10.0))
        # merge from-tdigest into to-tdigest
        self.assertOk(self.cmd("tdigest.merge", "to-tdigest", "from-tdigest"))
        # we should now have 1100 weight on to-histogram
        to_info = parse_tdigest_info(self.cmd("tdigest.info", "to-tdigest"))
        total_weight_to = float(to_info["Merged weight"]) + float(
            to_info["Unmerged weight"]
        )
        self.assertEqual(1100, total_weight_to)

    def test_tdigest_merge_to_empty(self):
        self.cmd("FLUSHALL")
        self.assertOk(self.cmd("tdigest.create", "to-tdigest", 100))
        self.assertOk(self.cmd("tdigest.create", "from-tdigest", 100))
        # insert datapoints into sketch
        for _ in range(100):
            self.assertOk(self.cmd("tdigest.add", "from-tdigest", 1.0, 1.0))
        # merge from-tdigest into to-tdigest
        self.assertOk(self.cmd("tdigest.merge", "to-tdigest", "from-tdigest"))
        # assert we have same merged weight on both histograms ( given the to-histogram was empty )
        from_info = parse_tdigest_info(self.cmd("tdigest.info", "from-tdigest"))
        total_weight_from = float(from_info["Merged weight"]) + float(
            from_info["Unmerged weight"]
        )
        to_info = parse_tdigest_info(self.cmd("tdigest.info", "to-tdigest"))
        total_weight_to = float(to_info["Merged weight"]) + float(
            to_info["Unmerged weight"]
        )
        self.assertEqual(total_weight_from, total_weight_to)

    def test_tdigest_mergestore(self):
        self.cmd("FLUSHALL")
        self.assertOk(self.cmd("tdigest.create", "from-1", 100))
        self.assertOk(self.cmd("tdigest.create", "from-2", 200))
        # insert datapoints into sketch
        self.assertOk(self.cmd("tdigest.add", "from-1", 1.0, 1.0))
        self.assertOk(self.cmd("tdigest.add", "from-2", 1.0, 10.0))
        # merge to a t-digest with default compression
        self.assertOk(self.cmd("tdigest.mergestore", "to-tdigest-100", "2","from-1", "from-2"))
        # assert we have same merged weight on both histograms ( given the to-histogram was empty )
        from_info = parse_tdigest_info(self.cmd("tdigest.info", "to-tdigest-100"))
        total_weight_to = float(from_info["Merged weight"]) + float(
            from_info["Unmerged weight"]
        )
        total_weight_from = 10.0 + 1.0
        self.assertEqual(total_weight_from, total_weight_to)

        # merge to a t-digest with default compression
        self.assertOk(self.cmd("tdigest.mergestore", "to-tdigest-100", "2","from-1", "from-2", "COMPRESSION", "200"))

    def test_tdigest_mergestore_percentile(self):
        self.cmd("FLUSHALL")
        self.assertOk(self.cmd("tdigest.create", "from-1", 500))
        # insert datapoints into sketch
        for x in range(1, 10000):
            self.assertOk(self.cmd("tdigest.add", "from-1", x * 0.01, 1.0))
        # merge to a t-digest with default compression
        self.assertOk(self.cmd("tdigest.mergestore", "to-tdigest-500", "1","from-1", "COMPRESSION", "500"))
        # assert min min/max have same result as quantile 0 and 1
        self.assertEqual(
            float(self.cmd("tdigest.max", "to-tdigest-500")),
            float(self.cmd("tdigest.quantile", "to-tdigest-500", 1.0)[1]),
        )
        self.assertEqual(
            float(self.cmd("tdigest.min", "to-tdigest-500")),
            float(self.cmd("tdigest.quantile", "to-tdigest-500", 0.0)[1]),
        )
        self.assertAlmostEqual(
            1.0, float(self.cmd("tdigest.quantile", "to-tdigest-500", 0.01)[1]), 0.01
        )
        self.assertAlmostEqual(
            99.0, float(self.cmd("tdigest.quantile", "to-tdigest-500", 0.99)[1]), 0.01
        )
        self.assertAlmostEqual(
            99.0, float(self.cmd("tdigest.quantile", "to-tdigest-500", 0.01, 0.99)[3]), 0.01
        )
        expected = [0.01,1.0,0.50,50.0,0.95,95.0,0.99,99.0]
        res = self.cmd("tdigest.quantile", "to-tdigest-500", 0.01, 0.5, 0.95, 0.99)
        for pos, v in enumerate(res):
            self.assertAlmostEqual(
                expected[pos], float(v), 0.01
            )

    def test_negative_tdigest_merge(self):
        self.cmd("SET", "to-tdigest", "B")
        self.cmd("SET", "from-tdigest", "B")

        # WRONGTYPE
        self.assertRaises(
            ResponseError, self.cmd, "tdigest.merge", "to-tdigest", "from-tdigest"
        )
        self.cmd("DEL", "to-tdigest")
        self.assertOk(self.cmd("tdigest.create", "to-tdigest", 100))
        self.assertRaises(
            ResponseError, self.cmd, "tdigest.merge", "to-tdigest", "from-tdigest"
        )
        self.cmd("DEL", "from-tdigest")
        self.assertOk(self.cmd("tdigest.create", "from-tdigest", 100))
        # arity lower
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.merge", "to-tdigest"
        )
        # arity upper
        self.assertRaises(
            ResponseError,
            self.cmd,
            "tdigest.merge",
            "to-tdigest",
            "from-tdigest",
            "from-tdigest",
        )
        # key does not exist
        self.assertRaises(
            ResponseError, self.cmd, "tdigest.merge", "dont-exist", "to-tdigest"
        )
        self.assertRaises(
            ResponseError, self.cmd, "tdigest.merge", "to-tdigest", "dont-exist"
        )

    def test_negative_tdigest_mergestore(self):
        self.cmd("SET", "to-tdigest", "B")
        self.cmd("SET", "from-tdigest", "B")

        # WRONGTYPE
        self.assertRaises(
            ResponseError, self.cmd, "tdigest.mergestore", "to-tdigest", "1", "from-tdigest"
        )
        self.cmd("DEL", "to-tdigest")
        # arity lower
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.mergestore"
        )
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.mergestore", "to-tdigest"
        )
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.mergestore", "to-tdigest", "1"
        )
        # arity upper
        self.assertRaises(
            ResponseError,
            self.cmd,
            "tdigest.mergestore",
            "to-tdigest",
            "1",
            "from-tdigest",
            "extra-arg",
        )

    def test_tdigest_min_max(self):
        self.assertOk(self.cmd("tdigest.create", "tdigest", 100))
        # test for no datapoints first
        self.assertEqual(sys.float_info.max, float(self.cmd("tdigest.min", "tdigest")))
        self.assertEqual(-sys.float_info.max, float(self.cmd("tdigest.max", "tdigest")))
        # insert datapoints into sketch
        for x in range(1, 101):
            self.assertOk(self.cmd("tdigest.add", "tdigest", x, 1.0))
        # min/max
        self.assertEqual(100, float(self.cmd("tdigest.max", "tdigest")))
        self.assertEqual(1, float(self.cmd("tdigest.min", "tdigest")))

    def test_negative_tdigest_min_max(self):
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

        self.cmd("DEL", "tdigest", "B")
        self.assertOk(self.cmd("tdigest.create", "tdigest", 100))
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
        self.assertOk(self.cmd("tdigest.create", "tdigest", 500))
        # insert datapoints into sketch
        for x in range(1, 10000):
            self.assertOk(self.cmd("tdigest.add", "tdigest", x * 0.01, 1.0))
        # assert min min/max have same result as quantile 0 and 1
        self.assertEqual(
            float(self.cmd("tdigest.max", "tdigest")),
            float(self.cmd("tdigest.quantile", "tdigest", 1.0)[1]),
        )
        self.assertEqual(
            float(self.cmd("tdigest.min", "tdigest")),
            float(self.cmd("tdigest.quantile", "tdigest", 0.0)[1]),
        )
        self.assertAlmostEqual(
            1.0, float(self.cmd("tdigest.quantile", "tdigest", 0.01)[1]), 0.01
        )
        self.assertAlmostEqual(
            99.0, float(self.cmd("tdigest.quantile", "tdigest", 0.99)[1]), 0.01
        )
        self.assertAlmostEqual(
            99.0, float(self.cmd("tdigest.quantile", "tdigest", 0.01, 0.99)[3]), 0.01
        )
        expected = [0.01,1.0,0.50,50.0,0.95,95.0,0.99,99.0]
        res = self.cmd("tdigest.quantile", "tdigest", 0.01, 0.5, 0.95, 0.99)
        for pos, v in enumerate(res):
            self.assertAlmostEqual(
                expected[pos], float(v), 0.01
            )
        # the reply provides the output percentiles in ordered manner
        res = self.cmd("tdigest.quantile", "tdigest", 0.95, 0.99, 0.01, 0.5)
        for pos, v in enumerate(res):
            self.assertAlmostEqual(
                expected[pos], float(v), 0.01
            )

    def test_negative_tdigest_quantile(self):
        self.cmd("SET", "tdigest", "B")
        # WRONGTYPE
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.quantile", "tdigest", 0.9
        )
        # key does not exist
        self.assertRaises(
            ResponseError, self.cmd, "tdigest.quantile", "dont-exist", 0.9
        )
        self.cmd("DEL", "tdigest", "B")
        self.assertOk(self.cmd("tdigest.create", "tdigest", 100))
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
        # parsing
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.quantile", "tdigest", "a"
        )

    def test_tdigest_cdf(self):
        self.assertOk(self.cmd("tdigest.create", "tdigest", 500))
        # insert datapoints into sketch
        for x in range(1, 100):
            self.assertOk(self.cmd("tdigest.add", "tdigest", x, 1.0))

        self.assertAlmostEqual(
            0.01, float(self.cmd("tdigest.cdf", "tdigest", 1.0)), 0.01
        )
        self.assertAlmostEqual(
            0.99, float(self.cmd("tdigest.cdf", "tdigest", 99.0)), 0.01
        )

    def test_negative_tdigest_cdf(self):
        self.cmd("SET", "tdigest", "B")
        # WRONGTYPE
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.cdf", "tdigest", 0.9
        )
        # key does not exist
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.cdf", "dont-exist", 0.9
        )
        self.cmd("DEL", "tdigest", "B")
        self.assertOk(self.cmd("tdigest.create", "tdigest", 100))
        # arity lower
        self.assertRaises(redis.exceptions.ResponseError, self.cmd, "tdigest.cdf")
        # arity upper
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.cdf", "tdigest", 1, 1
        )
        # parsing
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.cdf", "tdigest", "a"
        )

    def test_tdigest_trimmed_mean(self):
        self.assertOk(self.cmd("tdigest.create", "tdigest", 500))
        # insert datapoints into sketch
        for x in range(0, 20):
            self.assertOk(self.cmd("tdigest.add", "tdigest", x, 1.0))

        self.assertAlmostEqual(
            9.5, float(self.cmd("tdigest.trimmed_mean", "tdigest", 0.1,0.9)), 0.01
        )
        self.assertAlmostEqual(
            9.5, float(self.cmd("tdigest.trimmed_mean", "tdigest", 0.0,1.0)), 0.01
        )
        self.assertAlmostEqual(
            9.5, float(self.cmd("tdigest.trimmed_mean", "tdigest", 0.2,0.8)), 0.01
        )

    def test_negative_tdigest_trimmed_mean(self):
        self.cmd("SET", "tdigest", "B")
        # WRONGTYPE
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.trimmed_mean", "tdigest", 0.9
        )
        # key does not exist
        self.assertRaises(
            ResponseError, self.cmd, "tdigest.trimmed_mean", "dont-exist", 0.9
        )
        self.cmd("DEL", "tdigest", "B")
        self.assertOk(self.cmd("tdigest.create", "tdigest", 100))
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

    def test_negative_tdigest_info(self):
        self.cmd("SET", "tdigest", "B")
        # WRONGTYPE
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.info", "tdigest"
        )
        # dont exist
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.info", "dont-exist"
        )
        self.cmd("DEL", "tdigest", "B")
        self.assertOk(self.cmd("tdigest.create", "tdigest", 100))
        # arity lower
        self.assertRaises(redis.exceptions.ResponseError, self.cmd, "tdigest.info")
        # arity upper
        self.assertRaises(
            redis.exceptions.ResponseError, self.cmd, "tdigest.info", "tdigest", 1
        )

    def test_save_load(self):
        self.assertOk(self.cmd("tdigest.create", "tdigest", 500))
        # insert datapoints into sketch
        for _ in range(1, 101):
            self.assertOk(self.cmd("tdigest.add", "tdigest", 1.0, 1.0))
        self.assertEqual(True, self.cmd("SAVE"))
        mem_usage_prior_restart = self.cmd("MEMORY", "USAGE", "tdigest")
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
