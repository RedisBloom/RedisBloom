from docs_utils import *

class testCommandDocsAndHelp():
    def __init__(self):
        self.env = Env(decodeResponses=True)

    def test_command_docs_cf_reserve(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'cf.reserve',
            summary='Creates a new Cuckoo Filter',
            complexity='O(1)',
            arity=-3,
            since='1.0.0',
            args=[
                ('key', 'key'),
                ('capacity', 'integer'),
                ('bucketsize', 'block'),
                ('maxiterations', 'block'),
                ('expansion', 'block'),
            ],
            key_pos=1,
        )

    def test_command_docs_bf_add(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'bf.add',
            summary='Adds an item to a Bloom filter.',
            complexity='O(k), where k is the number of hash functions used by the last sub-filter',
            arity=3,
            since='1.0.0',
            args=[('key', 'key'), ('item', 'string')],
            key_pos=1,
        )

    def test_command_docs_bf_exists(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'bf.exists',
            summary='Checks whether an item exists in a Bloom Filter',
            complexity='O(k), where k is the number of hash functions used by the last sub-filter',
            arity=3,
            since='1.0.0',
            args=[('key', 'key'), ('item', 'string')],
            key_pos=1,
        )

    def test_command_docs_bf_info(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'bf.info',
            summary='Returns information about a Bloom Filter',
            complexity='O(1)',
            arity=2,
            since='1.0.0',
            args=[('key', 'key'), ('single_value', 'oneof', [('capacity', 'pure-token', 'CAPACITY'), ('size', 'pure-token', 'SIZE'), ('filters', 'pure-token', 'FILTERS'), ('items', 'pure-token', 'ITEMS'), ('expansion', 'pure-token', 'EXPANSION')])],
            key_pos=1,
        )

    def test_command_docs_bf_card(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'bf.card',
            summary='Returns the cardinality of a Bloom filter',
            complexity='O(1)',
            arity=2,
            since='2.4.4',
            args=[('key', 'key')],
            key_pos=1,
        )

    def test_command_docs_bf_insert(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        
        assert_docs(
            env,
            "bf.insert",
            summary="Adds one or more items to a Bloom Filter. A filter will be created if it does not exist",
            complexity="O(k * n), where k is the number of hash functions and n is the number of items",
            arity=4,
            since="1.0.0",
            args=[("key", "key"), ("capacity", "block"), ("error", "block"), ("expansion", "block"), ("nocreate", "pure-token"), ("nonscaling", "pure-token"), ("items", "pure-token"), ("item", "string")],
            key_pos=1,
        )
