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
            summary="Adds an item to a Bloom Filter",
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
            summary='Adds an item to a Top-k sketch. Multiple items can be added at the same time.',
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
            summary='Returns information about a sketch',
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
            summary="Return the full list of items in Top-K sketch.",
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
            summary='Checks whether one or more items are in a sketch',
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
            summary='Increases the count of one or more items by increment',
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
            summary='Returns information about a sketch',
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
            summary='Merges several sketches into one sketch',
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

    def test_command_docs_cf_add(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'cf.add',
            summary="Adds an item to a Cuckoo Filter",
            complexity='O(k + i), where k is the number of sub-filters and i is maxIterations',
            arity=3,
            since='1.0.0',
            args=[('key', 'key'), ('item', 'string')],
            key_pos=1,
        )

    def test_command_docs_cf_addnx(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'cf.addnx',
            summary="Adds an item to a Cuckoo Filter if the item did not exist previously.",
            complexity='O(k + i), where k is the number of sub-filters and i is maxIterations',
            arity=3,
            since='1.0.0',
            args=[('key', 'key'), ('item', 'string')],
            key_pos=1,
        )

    def test_command_docs_cf_count(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'cf.count',
            summary='Return the number of times an item might be in a Cuckoo Filter',
            complexity='O(k), where k is the number of sub-filters',
            arity=-3,
            since='1.0.0',
            args=[('key', 'key'), ('item', 'string')],
            key_pos=1,
        )

    def test_command_docs_cf_del(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'cf.del',
            summary='Deletes an item from a Cuckoo Filter',
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
            summary='Checks whether one or more items exist in a Cuckoo Filter',
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
            summary='Returns information about a Cuckoo Filter',
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
            summary='Adds one or more items to a Cuckoo Filter. A filter will be created if it does not exist',
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
            summary='Adds one or more items to a Cuckoo Filter if the items did not exist previously. A filter will be created if it does not exist',
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
            summary='Restores a filter previously saved using SCANDUMP',
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
            summary='Checks whether one or more items exist in a Cuckoo Filter',
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
            summary='Begins an incremental save of the bloom filter',
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
            summary='Adds one or more observations to a t-digest sketch',
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
            summary='Returns, for each input rank, an estimation of the value (floating-point) with that rank',
            complexity='O(N) where N is the number of ranks specified',
            arity=-3,
            since='2.4.0',
            args=[('key', 'key'), ('rank', 'double')],
            key_pos=1,
        )

    def test_command_docs_tdigest_byrevrank(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'tdigest.byrevrank',
            summary='Returns, for each input reverse rank, an estimation of the value (floating-point) with that reverse rank',
            complexity='O(N) where N is the number of reverse ranks specified.',
            arity=-3,
            since='2.4.0',
            args=[('key', 'key'), ('reverse_rank', 'double')],
            key_pos=1,
        )

    def test_command_docs_tdigest_cdf(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'tdigest.cdf',
            summary='Returns, for each input value, an estimation of the floating-point fraction of (observations smaller than the given value + half the observations equal to the given value). Multiple fractions can be retrieved in a single call.',
            complexity='O(N) where N is the number of values specified.',
            arity=-3,
            since='2.4.0',
            args=[('key', 'key'), ('value', 'double')],
            key_pos=1,
        )

    def test_command_docs_tdigest_create(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'tdigest.create',
            summary='Allocates memory and initializes a new t-digest sketch',
            complexity='O(1)',
            arity=-2,
            since='2.4.0',
            args=[('key', 'key'), ('compression_block', 'block')],
            key_pos=1,
        )

    def test_command_docs_tdigest_info(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'tdigest.info',
            summary='Returns information and statistics about a t-digest sketch',
            complexity='O(1)',
            arity=-2,
            since='2.4.0',
            args=[('key', 'key')],
            key_pos=1,
        )

    def test_command_docs_tdigest_max(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'tdigest.max',
            summary='Returns the maximum observation value from a t-digest sketch',
            complexity='O(1)',
            arity=2,
            since='2.4.0',
            args=[('key', 'key')],
            key_pos=1,
        )

    def test_command_docs_tdigest_merge(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'tdigest.merge',
            summary='Merges multiple t-digest sketches into a single sketch',
            complexity='O(N*K), where N is the number of centroids and K being the number of input sketches',
            arity=-4,
            since='2.4.0',
            args=[('destination-key', 'key'), ('numkeys', 'integer'), ('source-key', 'key'), ('compression_block', 'block'), ('override', 'pure-token')],
            key_pos=1,
        )

    def test_command_docs_tdigest_min(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'tdigest.min',
            summary='Returns the minimum observation value from a t-digest sketch',
            complexity='O(1)',
            arity=2,
            since='2.4.0',
            args=[('key', 'key')],
            key_pos=1,
        )

    def test_command_docs_tdigest_quantile(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'tdigest.quantile',
            summary='Returns, for each input fraction, an estimation of the value (floating point) that is smaller than the given fraction of observations',
            complexity='O(N) where N is the number of quantiles specified.',
            arity=-3,
            since='2.4.0',
            args=[('key', 'key'), ('quantile', 'double')],
            key_pos=1,
        )

    def test_command_docs_tdigest_rank(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'tdigest.rank',
            summary='Returns, for each input value (floating-point), the estimated rank of the value (the number of observations in the sketch that are smaller than the value + half the number of observations that are equal to the value)',
            complexity='O(N) where N is the number of values specified.',
            arity=-3,
            since='2.4.0',
            args=[('key', 'key'), ('value', 'double')],
            key_pos=1,
        )

    def test_command_docs_tdigest_reset(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'tdigest.reset',
            summary='Resets a t-digest sketch: empty the sketch and re-initializes it.',
            complexity='O(1)',
            arity=2,
            since='2.4.0',
            args=[('key', 'key')],
            key_pos=1,
        )

    def test_command_docs_tdigest_revrank(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'tdigest.revrank',
            summary='Returns, for each input value (floating-point), the estimated reverse rank of the value (the number of observations in the sketch that are larger than the value + half the number of observations that are equal to the value)',
            complexity="O(N) where N is the number of values specified.",
            arity=-3,
            since='2.4.0',
            args=[('key', 'key'), ('value', 'double')],
            key_pos=1,
        )

    def test_command_docs_tdigest_trimmed_mean(self):
        env = self.env
        if server_version_less_than(env, '7.0.0'):
            env.skip()
        assert_docs(
            env, 'tdigest.trimmed_mean',
            summary='Returns an estimation of the mean value from the sketch, excluding observation values outside the low and high cutoff quantiles',
            complexity="O(N) where N is the number of centroids",
            arity=4,
            since='2.4.0',
            args=[('key', 'key'), ('low_cut_quantile', 'double'), ('high_cut_quantile', 'double')],
            key_pos=1,
        )

