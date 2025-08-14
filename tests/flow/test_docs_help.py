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
