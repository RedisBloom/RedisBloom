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
            arity=-2,
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
            arity=-4,
            since="1.0.0",
            args=[("key", "key"), ("capacity", "block"), ("error", "block"), ("expansion", "block"), ("nocreate", "pure-token"), ("nonscaling", "pure-token"), ("items", "pure-token"), ("item", "string")],
            key_pos=1,
        )

    def test_command_docs_bf_loadchunk(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'bf.loadchunk',
            summary='Restores a filter previously saved using SCANDUMP',
            complexity='O(n), where n is the capacity',
            arity=4,
            since='1.0.0',
            args=[('key', 'key'), ('iterator', 'integer'), ('data', 'string')],
            key_pos=1,
        )

    def test_command_docs_bf_madd(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'bf.madd',
            summary='Adds one or more items to a Bloom Filter. A filter will be created if it does not exist',
            complexity='O(k * n), where k is the number of hash functions and n is the number of items',
            arity=-3,
            since='1.0.0',
            args=[('key', 'key'), ('item', 'string')],
            key_pos=1,
        )

    def test_command_docs_bf_mexists(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'bf.mexists',
            summary='Checks whether one or more items exist in a Bloom Filter',
            complexity='O(k * n), where k is the number of hash functions and n is the number of items',
            arity=-3,
            since='1.0.0',
            args=[('key', 'key'), ('item', 'string')],
            key_pos=1,
        )
    
    def test_command_docs_bf_reserve(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'bf.reserve',
            summary='Creates a new Bloom Filter',
            complexity='O(1)',
            arity=-4,
            since='1.0.0',
            args=[('key', 'key'), ('error_rate', 'double'), ('capacity', 'integer'), ('expansion', 'block'), ('nonscaling', 'pure-token')],
            key_pos=1,
        )
    
    def test_command_docs_bf_scandump(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'bf.scandump',
            summary='Begins an incremental save of the bloom filter',
            complexity='O(n), where n is the capacity',
            arity=3,
            since='1.0.0',
            args=[('key', 'key'), ('iterator', 'integer')],
            key_pos=1,
        )

    def test_command_docs_topk_add(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'topk.add',
            summary='Adds an item to a Top-k sketch. Multiple items can be added at the same time. If an item enters the Top-K sketch, the item that is expelled (if any) is returned',
            complexity='O(n * k) where n is the number of items and k is the depth',
            arity=-3,
            since='2.0.0',
            args=[('key', 'key'), ('item', 'string')],
            key_pos=1,
        )

    def test_command_docs_topk_count(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'topk.count',
            summary='Return the count for one or more items are in a sketch',
            complexity='O(n) where n is the number of items',
            arity=-3,
            since='2.0.0',
            args=[('key', 'key'), ('item', 'string')],
            key_pos=1,
        )
    
    def test_command_docs_topk_incrby(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'topk.incrby',
            summary='Increases the count of one or more items by increment',
            complexity='O(n * k * incr) where n is the number of items, k is the depth and incr is the increment',
            arity=-4,
            since='2.0.0',
            args=[('key', 'key'), ('item_increment', 'block')],
            key_pos=1,
        )

    def test_command_docs_topk_info(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'topk.info',
            summary='Returns number of required items (k), width, depth, and decay values of a given sketch',
            complexity='O(1)',
            arity=2,
            since='2.0.0',
            args=[('key', 'key')],
            key_pos=1,
        )

    def test_command_docs_topk_list(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'topk.list',
            summary='Return the full list of items in Top-K sketch',
            complexity='O(k*log(k)) where k is the value of top-k',
            arity=-2,
            since='2.0.0',
            args=[('key', 'key'), ('withcount', 'pure-token')],
            key_pos=1,
        )

    def test_command_docs_topk_query(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'topk.query',
            summary='Checks whether one or more items are one of the Top-K items',
            complexity='O(n) where n is the number of items',
            arity=-3,
            since='2.0.0',
            args=[('key', 'key'), ('item', 'string')],
            key_pos=1,
        )
    
    def test_command_docs_topk_reserve(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'topk.reserve',
            summary='Initializes a Top-K sketch with specified parameters',
            complexity='O(1)',
            arity=-3,
            since='2.0.0',
            args=[('key', 'key'), ('topk', 'integer'), ('params', 'block')],
            key_pos=1,
        )

    def test_command_docs_cms_incrby(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'cms.incrby',
            summary='Increases the count of item by increment. Multiple items can be increased with one call.',
            complexity='O(n) where n is the number of items',
            arity=-4,
            since='2.0.0',
            args=[('key', 'key'), ('items', 'block')],
            key_pos=1,
        )

    def test_command_docs_cms_info(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'cms.info',
            summary='Returns width, depth and total count of the sketch',
            complexity='O(1)',
            arity=2,
            since='2.0.0',
            args=[('key', 'key')],
            key_pos=1,
        )
    
    def test_command_docs_cms_initbydim(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'cms.initbydim',
            summary='Initializes a Count-Min Sketch to dimensions specified by user',
            complexity='O(1)',
            arity=4,
            since='2.0.0',
            args=[('key', 'key'), ('width', 'integer'), ('depth', 'integer')],
            key_pos=1,
        )

    def test_command_docs_cms_initbyprob(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'cms.initbyprob',
            summary='Initializes a Count-Min Sketch to accommodate requested tolerances.',
            complexity='O(1)',
            arity=4,
            since='2.0.0',
            args=[('key', 'key'), ('error', 'double'), ('probability', 'double')],
            key_pos=1,
        )

    def test_command_docs_cms_merge(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'cms.merge',
            summary='Merges several sketches into one sketch. All sketches must have identical width and depth. Weights can be used to multiply certain sketches. Default weight is 1.',
            complexity='O(n) where n is the number of sketches',
            arity=-4,
            since='2.0.0',
            args=[('key', 'key'), ('numKeys', 'integer'), ('source', 'key'), ('weights', 'block')],
            key_pos=1,
        )

    def test_command_docs_cms_query(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'cms.query',
            summary='Returns the count for one or more items in a sketch',
            complexity='O(n) where n is the number of items',
            arity=-3,
            since='2.0.0',
            args=[('key', 'key'), ('item', 'string')],
            key_pos=1,
        )
        
    def test_command_docs_cf_del(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'cf.del',
            summary='Deletes an item once from the filter. If the item exists only once, it will be removed from the filter. If the item was added multiple times, it will still be present.',
            complexity='O(k), where k is the number of sub-filters',
            arity=3,
            since='1.0.0',
            args=[('key', 'key'), ('item', 'string')],
            key_pos=1,
        )

    def test_command_docs_cf_exists(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'cf.exists',
            summary='Determines whether a given item was added to a cuckoo filter.',
            complexity='O(k), where k is the number of sub-filters',
            arity=3,
            since='1.0.0',
            args=[('key', 'key'), ('item', 'string')],
            key_pos=1,
        )

    def test_command_docs_cf_info(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'cf.info',
            summary='Returns information about a cuckoo filter.',
            complexity='O(1)',
            arity=2,
            since='1.0.0',
            args=[('key', 'key')],
            key_pos=1,
        )

    def test_command_docs_cf_insert(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'cf.insert',
            summary='Adds one or more items to a cuckoo filter, allowing the filter to be created with a custom capacity if it does not exist yet. This command is similar to CF.ADD, except that more than one item can be added and capacity can be specified.',
            complexity='O(n * (k + i)), where n is the number of items, k is the number of sub-filters and i is maxIterations',
            arity=-4,
            since='1.0.0',
            args=[('key', 'key'), ('capacity', 'block'), ('nocreate', 'pure-token'), ('items', 'pure-token'), ('item', 'string')],
            key_pos=1,
        )

    def test_command_docs_cf_insertnx(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'cf.insertnx',
            summary='Adds one or more items to a cuckoo filter if they did not exist previously, allowing the filter to be created with a custom capacity if it does not exist yet.',
            complexity='O(n * (k + i)), where n is the number of items, k is the number of sub-filters and i is maxIterations',
            arity=-4,
            since='1.0.0',
            args=[('key', 'key'), ('capacity', 'block'), ('nocreate', 'pure-token'), ('items', 'pure-token'), ('item', 'string')],
            key_pos=1,
        )

    def test_command_docs_cf_loadchunk(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'cf.loadchunk',
            summary='Restores a cuckoo filter previously saved using CF.SCANDUMP.',
            complexity='O(n), where n is the capacity',
            arity=4,
            since='1.0.0',
            args=[('key', 'key'), ('iterator', 'integer'), ('data', 'string')],
            key_pos=1,
        )

    def test_command_docs_cf_mexists(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'cf.mexists',
            summary='Determines whether a given item was added to a cuckoo filter.',
            complexity='O(k * n), where k is the number of sub-filters and n is the number of items',
            arity=-3,
            since='1.0.0',
            args=[('key', 'key'), ('item', 'string')],
            key_pos=1,
        )

    def test_command_docs_cf_scandump(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'cf.scandump',
            summary='Begins an incremental save of the cuckoo filter. The first time this command is called, the value of iter should be 0.',
            complexity='O(n), where n is the capacity',
            arity=3,
            since='1.0.0',
            args=[('key', 'key'), ('iterator', 'integer')],
            key_pos=1,
        )

    def test_command_docs_tdigest_add(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'tdigest.add',
            summary='Adds one or more observations to a t-digest sketch.',
            complexity='O(N), where N is the number of samples to add',
            arity=-3,
            since='2.4.0',
            args=[('key', 'key'), ('value', 'double')],
            key_pos=1,
        )

    def test_command_docs_tdigest_byrank(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'tdigest.byrank',
            summary='Returns the value at the given rank in the t-digest sketch.',
            complexity='O(N) where N is the number of ranks specified',
            arity=-3,
            since='2.4.0',
            args=[('key', 'key'), ('rank', 'double')],
            key_pos=1,
        )
