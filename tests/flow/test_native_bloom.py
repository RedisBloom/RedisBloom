from common import Env


def test_native_bloom_coexists_with_module():
    env = Env(decodeResponses=True)
    commands = [command.lower() for command in
                env.cmd('COMMAND LIST', 'FILTERBY', 'MODULE', 'bf')]
    env.assertFalse(any(command.startswith('bf.') for command in commands))
    for command in ('cf.add', 'cms.incrby', 'topk.add', 'tdigest.add'):
        env.assertTrue(command in commands)

    env.cmd('CONFIG', 'SET', 'bf-initial-size', 123)
    env.cmd('BF.ADD', 'bloom', 'item')
    env.assertEqual(env.cmd('TYPE', 'bloom'), 'bloom')
    env.assertEqual(env.cmd('BF.INFO', 'bloom', 'CAPACITY'), [123])
    env.cmd('CF.ADD', 'cuckoo', 'item')
    env.cmd('CMS.INITBYDIM', 'cms', 100, 5)
    env.cmd('CMS.INCRBY', 'cms', 'item', 3)
    env.cmd('TOPK.RESERVE', 'topk', 3)
    env.cmd('TOPK.ADD', 'topk', 'item')
    env.cmd('TDIGEST.CREATE', 'tdigest')
    env.cmd('TDIGEST.ADD', 'tdigest', 42)

    env.dumpAndReload()
    env.assertEqual(env.cmd('BF.EXISTS', 'bloom', 'item'), 1)
    env.assertEqual(env.cmd('CF.EXISTS', 'cuckoo', 'item'), 1)
    env.assertEqual(env.cmd('CMS.QUERY', 'cms', 'item'), [3])
    env.assertEqual(env.cmd('TOPK.QUERY', 'topk', 'item'), [1])
    env.assertEqual(float(env.cmd('TDIGEST.QUANTILE', 'tdigest', 0.5)[0]), 42)
