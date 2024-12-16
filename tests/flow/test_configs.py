from common import *

class testConfigs():
  def __init__(self):
    self.env = Env(decodeResponses=True)

  def test_default_configs(self):
      """Test that the various `bloom` config parameters were added appropriately in module load"""
      env = self.env
      res = env.cmd('CONFIG', 'GET', 'bf-error-rate')
      env.assertEqual(res[1], '0.010000')
      res = env.cmd('CONFIG', 'GET', 'bf-initial-size')
      env.assertEqual(res[1], '100')
      res = env.cmd('CONFIG', 'GET', 'bf-expansion-factor')
      env.assertEqual(res[1], '2')
      res = env.cmd('CONFIG', 'GET', 'cf-bucket-size')
      env.assertEqual(res[1], '2')
      res = env.cmd('CONFIG', 'GET', 'cf-initial-size')
      env.assertEqual(res[1], '1024')
      res = env.cmd('CONFIG', 'GET', 'cf-max-iterations')
      env.assertEqual(res[1], '20')
      res = env.cmd('CONFIG', 'GET', 'cf-expansion-factor')
      env.assertEqual(res[1], '1')
      res = env.cmd('CONFIG', 'GET', 'cf-max-expansions')
      env.assertEqual(res[1], '32')

  def test_config_set(self):
    """Test that the various `bloom` config parameters may be set"""
    env = self.env
    env.cmd('CONFIG', 'SET', 'bf-error-rate', '0.02')
    res = env.cmd('CONFIG', 'GET', 'bf-error-rate')
    env.assertEqual(res[1], '0.020000')
    env.cmd('CONFIG', 'SET', 'bf-initial-size', '200')
    res = env.cmd('CONFIG', 'GET', 'bf-initial-size')
    env.assertEqual(res[1], '200')
    env.cmd('CONFIG', 'SET', 'bf-expansion-factor', '3')
    res = env.cmd('CONFIG', 'GET', 'bf-expansion-factor')
    env.assertEqual(res[1], '3')
    env.cmd('CONFIG', 'SET', 'cf-bucket-size', '3')
    res = env.cmd('CONFIG', 'GET', 'cf-bucket-size')
    env.assertEqual(res[1], '3')
    env.cmd('CONFIG', 'SET', 'cf-initial-size', '2048')
    res = env.cmd('CONFIG', 'GET', 'cf-initial-size')
    env.assertEqual(res[1], '2048')
    env.cmd('CONFIG', 'SET', 'cf-max-iterations', '30')
    res = env.cmd('CONFIG', 'GET', 'cf-max-iterations')
    env.assertEqual(res[1], '30')
    env.cmd('CONFIG', 'SET', 'cf-expansion-factor', '2')
    res = env.cmd('CONFIG', 'GET', 'cf-expansion-factor')
    env.assertEqual(res[1], '2')
    env.cmd('CONFIG', 'SET', 'cf-max-expansions', '64')
    res = env.cmd('CONFIG', 'GET', 'cf-max-expansions')
    env.assertEqual(res[1], '64')

  def test_config_set_invalid(self):
    """Test that the various `bloom` config parameters may not be set to invalid values"""
    env = self.env
    # env.expect('CONFIG', 'SET', 'bf-error-rate', 0.0).error().contains('must be in the range') # TODO: 0.0 should not be allowed
    # env.expect('CONFIG', 'SET', 'bf-error-rate', 1.0).error().contains('must be in the range') # TODO: 1.0 should not be allowed
    env.expect('CONFIG', 'SET', 'bf-error-rate', 0.01).ok()
    env.expect('CONFIG', 'SET', 'bf-initial-size', 0).error().contains('must be in the range')
    env.expect('CONFIG', 'SET', 'bf-initial-size', 2**32).error().contains('must be in the range')
    env.expect('CONFIG', 'SET', 'bf-initial-size', 100).ok()
    env.expect('CONFIG', 'SET', 'bf-expansion-factor', -1).error().contains('must be in the range')
    env.expect('CONFIG', 'SET', 'bf-expansion-factor', 32769).error().contains('must be in the range')
    env.expect('CONFIG', 'SET', 'bf-expansion-factor', 2).ok()
    env.expect('CONFIG', 'SET', 'cf-bucket-size', 0).error().contains('must be in the range')
    env.expect('CONFIG', 'SET', 'cf-bucket-size', 256).error().contains('must be in the range')
    env.expect('CONFIG', 'SET', 'cf-bucket-size', 2).ok()
    env.expect('CONFIG', 'SET', 'cf-initial-size', 0).error().contains('must be in the range')
    env.expect('CONFIG', 'SET', 'cf-initial-size', 2**32).error().contains('must be in the range')
    env.expect('CONFIG', 'SET', 'cf-initial-size', 1024).ok()
    env.expect('CONFIG', 'SET', 'cf-max-iterations', 0).error().contains('must be in the range')
    env.expect('CONFIG', 'SET', 'cf-max-iterations', 65536).error().contains('must be in the range')
    env.expect('CONFIG', 'SET', 'cf-max-iterations', 20).ok()
    env.expect('CONFIG', 'SET', 'cf-expansion-factor', -1).error().contains('must be in the range')
    env.expect('CONFIG', 'SET', 'cf-expansion-factor', 32769).error().contains('must be in the range')
    env.expect('CONFIG', 'SET', 'cf-expansion-factor', 1).ok()
    env.expect('CONFIG', 'SET', 'cf-max-expansions', 0).error().contains('must be in the range')
    env.expect('CONFIG', 'SET', 'cf-max-expansions', 65537).error().contains('must be in the range')
    env.expect('CONFIG', 'SET', 'cf-max-expansions', 32).ok()

  def test_config_debug_stats(self):
    """Test that `bf.debug` and `cf.debug` config values reflect the current config values"""
    env = self.env

    # Default config values are 0.01, 100, 2
    env.cmd('bf.add', 'default_bf1', 'foo')
    default_bf1_debug_info = env.cmd('bf.debug', 'default_bf1')
    # Custom config values are 0.02, 200, 3
    env.cmd('bf.reserve', 'custom_bf1', 0.02, 200, 'expansion', 3)
    env.cmd('bf.add', 'custom_bf1', 'foo')
    custom_bf1_debug_info = env.cmd('bf.debug', 'custom_bf1')

    env.cmd('CONFIG', 'SET', 'bf-error-rate', 0.02, 'bf-initial-size', 200, 'bf-expansion-factor', 3)
    # Default config values are 0.02, 200, 3
    env.cmd('bf.add', 'default_bf2', 'foo')
    default_bf2_debug_info = env.cmd('bf.debug', 'default_bf2')
    # Custom config values are 0.01, 100, 2
    env.cmd('bf.reserve', 'custom_bf2', 0.01, 100, 'expansion', 2)
    env.cmd('bf.add', 'custom_bf2', 'foo')
    custom_bf2_debug_info = env.cmd('bf.debug', 'custom_bf2')

    env.assertEqual(default_bf1_debug_info, custom_bf2_debug_info)
    env.assertEqual(default_bf2_debug_info, custom_bf1_debug_info)
    env.assertNotEqual(default_bf1_debug_info, default_bf2_debug_info)
    env.assertNotEqual(custom_bf1_debug_info, custom_bf2_debug_info)
