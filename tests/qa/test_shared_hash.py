"""Cross-repository integration tests for the shared hash configuration.

REDIS_SERVER=/path/to/patched/redis-server REDISBLOOM_MODULE=/path/to/redisbloom.so \
    python -m unittest discover -s tests/qa -p test_shared_hash.py -v

Requires redis-py. Uses private temporary directories and dynamically selected ports.
"""
import contextlib
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import time
import unittest

import redis

SERVER = os.environ.get("REDIS_SERVER")
MODULE = os.environ.get("REDISBLOOM_MODULE")
SEED_A = "0123456789abcdef"
SEED_B = "fedcba9876543210"


def wait(predicate, timeout=10):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        if predicate():
            return
        time.sleep(0.02)
    raise AssertionError("Condition not reached before timeout")


class Node:
    def __init__(self, directory, seed=SEED_A, aof=False, cluster=False):
        self.directory = Path(directory)
        self.directory.mkdir(parents=True, exist_ok=True)
        with socket.socket() as sock:
            sock.bind(("127.0.0.1", 0))
            self.port = sock.getsockname()[1]
        self.cluster = cluster
        self.seed = seed
        self.aof = aof
        self.process = None
        self.client = redis.Redis(port=self.port, socket_timeout=2)
        self.log_path = self.directory / "redis.log"

    def start(self, expect_failure=False):
        args = [SERVER, "--bind", "127.0.0.1", "--port", str(self.port),
                "--dir", str(self.directory), "--save", "", "--logfile", str(self.log_path),
                "--enable-debug-command", "yes", "--repl-diskless-sync-delay", "0",
                "--loadmodule", MODULE, "--bf-hash-seed", self.seed,
                "--appendonly", "yes" if self.aof else "no", "--appendfsync", "always"]
        if self.cluster:
            with socket.socket() as sock:
                sock.bind(("127.0.0.1", 0))
                bus_port = sock.getsockname()[1]
            self.bus_port = bus_port
            args += ["--cluster-enabled", "yes", "--cluster-port", str(bus_port),
                     "--cluster-node-timeout", "10000"]
        self.process = subprocess.Popen(args, stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
        if expect_failure:
            self.process.wait(timeout=10)
            if self.process.returncode == 0:
                raise AssertionError("Expected startup to fail")
            return self

        def ready():
            if self.process.poll() is not None:
                raise AssertionError(self.log_path.read_text())
            try:
                return self.client.ping()
            except redis.ConnectionError:
                return False
        wait(ready)
        return self

    def stop(self):
        self.client.close()
        if self.process and self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()


@unittest.skipUnless(SERVER and MODULE, "Set REDIS_SERVER and REDISBLOOM_MODULE")
class SharedHashTests(unittest.TestCase):
    def setUp(self):
        self.stack = contextlib.ExitStack()
        self.addCleanup(self.stack.close)
        self.tmp = Path(self.stack.enter_context(tempfile.TemporaryDirectory()))
        self.serial = 0

    def node(self, **kwargs):
        self.serial += 1
        node = Node(self.tmp / str(self.serial), **kwargs)
        self.stack.callback(node.stop)
        return node.start()

    def populate(self, c, prefix=""):
        c.execute_command("BF.RESERVE", prefix + "bf", 0.001, 2)
        c.execute_command("CF.RESERVE", prefix + "cf", 64)
        c.execute_command("CMS.INITBYDIM", prefix + "cms", 100, 4)
        c.execute_command("TOPK.RESERVE", prefix + "topk", 10)
        for i in range(10):
            item = "item" + str(i)
            c.execute_command("BF.ADD", prefix + "bf", item)
            c.execute_command("CF.ADD", prefix + "cf", item)
            c.execute_command("CMS.INCRBY", prefix + "cms", item, 3)
            c.execute_command("TOPK.ADD", prefix + "topk", item)

    def check(self, c, prefix=""):
        for i in range(10):
            item = "item" + str(i)
            self.assertEqual(1, c.execute_command("BF.EXISTS", prefix + "bf", item))
            self.assertEqual(1, c.execute_command("CF.EXISTS", prefix + "cf", item))
            self.assertEqual([3], c.execute_command("CMS.QUERY", prefix + "cms", item))
            self.assertEqual([1], c.execute_command("TOPK.QUERY", prefix + "topk", item))

    def test_roundtrip_merge_and_chunks(self):
        for seed in ("", SEED_A):
            with self.subTest(seed=seed):
                a, b = self.node(seed=seed), self.node(seed=seed)
                self.populate(a.client)
                for key in ("bf", "cf", "cms", "topk"):
                    b.client.restore(key, 0, a.client.dump(key))
                self.check(b.client)
                for kind in ("BF", "CF"):
                    cursor = 0
                    while True:
                        cursor, data = a.client.execute_command(kind + ".SCANDUMP", kind.lower(), cursor)
                        if not cursor:
                            break
                        b.client.execute_command(kind + ".LOADCHUNK", kind + "copy", cursor, data)
                    self.assertEqual(1, b.client.execute_command(kind + ".EXISTS", kind + "copy", "item0"))
                b.client.execute_command("CMS.INITBYDIM", "dest", 100, 4)
                b.client.execute_command("CMS.MERGE", "dest", 2, "cms", "cms")
                self.assertEqual([6], b.client.execute_command("CMS.QUERY", "dest", "item0"))
                a.client.save()
                a.stop()
                a.start()
                self.check(a.client)

    def test_mismatched_restore_and_chunks(self):
        for source_seed, destination_seed in ((SEED_A, SEED_B), (SEED_A, ""), ("", SEED_A)):
            with self.subTest(source=source_seed, destination=destination_seed):
                a, b = self.node(seed=source_seed), self.node(seed=destination_seed)
                self.populate(a.client)
                for key in ("bf", "cf", "cms", "topk"):
                    with self.assertRaises(redis.ResponseError):
                        b.client.restore(key, 0, a.client.dump(key))
                    self.assertFalse(b.client.exists(key))
                for kind in ("BF", "CF"):
                    cursor, data = a.client.execute_command(kind + ".SCANDUMP", kind.lower(), 0)
                    with self.assertRaises(redis.ResponseError):
                        b.client.execute_command(kind + ".LOADCHUNK", "copy", cursor, data)
                    self.assertFalse(b.client.exists("copy"))

    def test_aof_replay_and_rewrite(self):
        for rewrite in (False, True):
            with self.subTest(rewrite=rewrite):
                a = self.node(aof=True)
                self.populate(a.client)
                if rewrite:
                    a.client.bgrewriteaof()
                    wait(lambda: a.client.info("persistence")["aof_rewrite_in_progress"] == 0)
                a.stop()
                a.start()
                self.check(a.client)
                a.stop()
                a.seed = SEED_B
                a.start(expect_failure=True)
                self.assertIn("hash configuration", a.log_path.read_text())

    def test_matching_replication_and_promotion(self):
        a, b = self.node(), self.node()
        self.populate(a.client)
        b.client.replicaof("127.0.0.1", a.port)
        wait(lambda: b.client.info("replication")["master_link_status"] == "up")
        self.check(b.client)
        a.client.execute_command("BF.ADD", "created-after-sync", "new")
        wait(lambda: b.client.exists("created-after-sync"))
        self.assertEqual(1, b.client.execute_command("BF.EXISTS", "created-after-sync", "new"))
        b.client.replicaof("NO", "ONE")
        b.client.execute_command("CMS.INITBYDIM", "promoted", 100, 4)
        b.client.execute_command("CMS.MERGE", "promoted", 1, "cms")
        self.assertEqual([3], b.client.execute_command("CMS.QUERY", "promoted", "item0"))

    def test_replication_mismatch_preserves_destination(self):
        for source_seed, destination_seed in ((SEED_A, SEED_B), (SEED_A, ""), ("", SEED_A)):
            with self.subTest(source=source_seed, destination=destination_seed):
                a, b = self.node(seed=source_seed), self.node(seed=destination_seed)
                a.client.execute_command("BF.ADD", "source", "x")
                b.client.set("preserve", "original")
                b.client.replicaof("127.0.0.1", a.port)
                wait(lambda: "Module replication compatibility check failed" in b.log_path.read_text())
                self.assertEqual(b"original", b.client.get("preserve"))
                self.assertEqual("down", b.client.info("replication")["master_link_status"])
                self.assertEqual(0, a.client.info("stats")["sync_full"])

    def test_seed_is_immutable(self):
        a = self.node()
        with self.assertRaisesRegex(redis.ResponseError, "immutable"):
            a.client.config_set("bf-hash-seed", SEED_B)

    def test_asm_and_merge_after_slot_move(self):
        for destination_seed in (SEED_A, SEED_B, ""):
            with self.subTest(destination_seed=destination_seed):
                nodes = [self.node(cluster=True), self.node(cluster=True, seed=destination_seed), self.node(cluster=True)]
                a, b, c = nodes
                for n in (b, c):
                    a.client.execute_command("CLUSTER", "MEET", "127.0.0.1", n.port, n.bus_port)
                for n, first, last in ((a, 0, 5460), (b, 5461, 10922), (c, 10923, 16383)):
                    n.client.execute_command("CLUSTER", "ADDSLOTSRANGE", first, last)
                wait(lambda: all(n.client.cluster("INFO")["cluster_state"] == "ok" for n in nodes))
                self.populate(a.client, "{06S}:")  # Hash slot zero.
                task = b.client.execute_command("CLUSTER", "MIGRATION", "IMPORT", 0, 0)
                def status():
                    raw = b.client.execute_command("CLUSTER", "MIGRATION", "STATUS", "ID", task)[0]
                    return dict(zip(raw[::2], raw[1::2]))
                if destination_seed == SEED_A:
                    wait(lambda: status()[b"state"] == b"completed")
                    self.check(b.client, "{06S}:")
                    b.client.execute_command("CMS.INITBYDIM", "{06S}:new", 100, 4)
                    b.client.execute_command("CMS.MERGE", "{06S}:new", 1, "{06S}:cms")
                    self.assertEqual([3], b.client.execute_command("CMS.QUERY", "{06S}:new", "item0"))
                else:
                    wait(lambda: b"MODULECONFIG" in status()[b"last_error"])
                    self.check(a.client, "{06S}:")
                    self.assertEqual(0, b.client.execute_command("CLUSTER", "COUNTKEYSINSLOT", 0))
                    b.client.execute_command("CLUSTER", "MIGRATION", "CANCEL", "ID", task)

    def test_seed_parsing_and_canonical_identity(self):
        a, b = self.node(seed=SEED_A), self.node(seed=SEED_A.upper())
        self.assertEqual(a.client.info("modules")["module_replication_compatibility"],
                         b.client.info("modules")["module_replication_compatibility"])
        self.node(seed="0000000000000000")  # Zero is a valid explicit seed, not legacy mode.
        for invalid in ("random", "123", "0123456789abcdeg", "0123456789abcdef00"):
            self.serial += 1
            n = Node(self.tmp / str(self.serial), seed=invalid)
            self.stack.callback(n.stop)
            n.start(expect_failure=True)
            self.assertIn("16 hexadecimal digits", n.log_path.read_text())

    def test_corrupt_chunk_configuration_is_rejected(self):
        a = self.node()
        self.populate(a.client)
        for kind in ("BF", "CF"):
            cursor, payload = a.client.execute_command(kind + ".SCANDUMP", kind.lower(), 0)
            for suffix in (bytes([2]) + payload[-15:], payload[-16:-1]):
                with self.assertRaises(redis.ResponseError):
                    a.client.execute_command(kind + ".LOADCHUNK", "invalid", cursor, payload[:-16] + suffix)
                self.assertFalse(a.client.exists("invalid"))

    def test_seed_changes_filter_bits(self):
        nodes = [self.node(seed=SEED_A), self.node(seed=SEED_A.upper()), self.node(seed=SEED_B)]
        chunks = []
        for n in nodes:
            n.client.execute_command("BF.RESERVE", "filter", 0.001, 100)
            n.client.execute_command("BF.MADD", "filter", *["item" + str(i) for i in range(50)])
            cursor, _ = n.client.execute_command("BF.SCANDUMP", "filter", 0)
            _, bits = n.client.execute_command("BF.SCANDUMP", "filter", cursor)
            chunks.append(bits)
        self.assertEqual(chunks[0], chunks[1])
        self.assertNotEqual(chunks[0], chunks[2])
