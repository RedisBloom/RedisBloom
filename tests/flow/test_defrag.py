import time
from common import *

from RLTest import Defaults

def enableDefrag(env):
    version = env.cmd('info', 'server')['redis_version']
    if '6.0' in version:
        # skil on version 6.0 and defrag API for modules was not supported on this version.
        env.skip()

    # make defrag as aggressive as possible
    env.cmd('CONFIG', 'SET', 'hz', '100')
    env.cmd('CONFIG', 'SET', 'active-defrag-ignore-bytes', '1')
    env.cmd('CONFIG', 'SET', 'active-defrag-threshold-lower', '0')
    env.cmd('CONFIG', 'SET', 'active-defrag-cycle-min', '99')

    try:
        env.cmd('CONFIG', 'SET', 'activedefrag', 'yes')
    except Exception:
        # If active defrag is not supported by the current Redis, simply skip the test.
        env.skip()

def testDefrag(env):
    enableDefrag(env)

    # Disable defrag so we can actually create fragmentation
    env.cmd('CONFIG', 'SET', 'activedefrag', 'no')
    
    # Create a key for each datatype
    for i in range(10000):
        env.expect('cms.initbydim', 'cms%d' % i, '20', '5').equal(b'OK')
        env.expect('cf.add', 'cf%d' % i, 'k1').equal(1)
        env.expect('bf.add', 'bf%d' % i, 'k1').equal(1)
        env.expect("tdigest.create", "tdigest%d" % i).equal(b'OK')
        env.expect("TDIGEST.ADD", "tdigest%d" % i, "20").equal(b'OK')
        env.expect('topk.reserve', 'topk%d' % i, '20', '50', '5', '0.9').equal(b'OK')
        env.expect('topk.add', 'topk%d' % i, 'a').equal([None])
    
    # Delete keys at even position
    for i in range(0, 10000, 2):
        env.expect('del', 'cms%d' % i).equal(1)
        env.expect('del', 'cf%d' % i).equal(1)
        env.expect('del', 'bf%d' % i).equal(1)
        env.expect("del", "tdigest%d" % i).equal(1)
        env.expect('del', 'topk%d' % i).equal(1)

    # wait for fragmentation for up to 30 seconds
    frag = env.cmd('info', 'memory')['allocator_frag_ratio']
    startTime = time.time()
    while frag < 1.4:
        time.sleep(0.1)
        frag = env.cmd('info', 'memory')['allocator_frag_ratio']
        if time.time() - startTime > 30:
            # We will wait for up to 30 seconds and then we consider it a failure
            env.assertTrue(False, message='Failed waiting for fragmentation, current value %s which is expected to be above 1.4.' % frag)
            return

    #enable active defrag
    env.cmd('CONFIG', 'SET', 'activedefrag', 'yes')

    # wait for fragmentation for go down for up to 30 seconds
    frag = env.cmd('info', 'memory')['allocator_frag_ratio']
    startTime = time.time()
    while frag > 1.1:
        time.sleep(0.1)
        frag = env.cmd('info', 'memory')['allocator_frag_ratio']
        if time.time() - startTime > 30:
            # We will wait for up to 30 seconds and then we consider it a failure
            env.assertTrue(False, message='Failed waiting for fragmentation to go down, current value %s which is expected to be bellow 1.1.' % frag)
            return
