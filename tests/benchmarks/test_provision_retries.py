"""Capacity retry tests; no AWS resources are created."""

from contextlib import ExitStack, redirect_stdout
import io
import json
import os
from pathlib import Path
import sys
import tempfile
from types import ModuleType
import unittest
from unittest.mock import Mock, patch

from python_terraform import TerraformCommandError
import compare_aws


class ProvisionRetryTests(unittest.TestCase):
    def setUp(self):
        self.stack = ExitStack()
        self.addCleanup(self.stack.close)
        self.directory = Path(self.stack.enter_context(tempfile.TemporaryDirectory()))
        self.state = self.directory / 'state.json'
        (self.directory / 'defaults.yml').write_text('remote: []\n')
        self.remote = ModuleType('redisbench_admin.utils.remote')
        self.remote.fetch_remote_setup_from_config = Mock(return_value=(str(self.directory), None, None))
        self.success = (0, 'ubuntu', 'private-server', 'public-server', 6379, 'private-client', 'public-client')
        self.remote.setup_remote_environment = Mock(return_value=self.success)
        self.stack.enter_context(patch.dict(sys.modules, {'redisbench_admin.utils.remote': self.remote}))
        self.stack.enter_context(patch.dict(os.environ, {
            'GITHUB_REPOSITORY': 'RedisBloom/RedisBloom', 'GITHUB_SHA': 'sha',
            'GITHUB_ACTOR': 'user', 'GITHUB_RUN_ID': '123', 'GITHUB_RUN_ATTEMPT': '1',
        }))
        self.stack.enter_context(patch('python_terraform.Terraform'))
        self.cleanup = self.stack.enter_context(patch.object(compare_aws, 'destroy'))
        self.now = 0
        self.stack.enter_context(patch.object(compare_aws.time, 'monotonic', side_effect=lambda: self.now))
        self.sleep = self.stack.enter_context(patch.object(compare_aws.time, 'sleep', side_effect=self.advance))
        self.console = self.stack.enter_context(redirect_stdout(io.StringIO()))

    def advance(self, seconds):
        self.now += seconds

    def capacity_error(self):
        return TerraformCommandError(1, 'terraform apply', '', 'api error InsufficientInstanceCapacity: unavailable')

    def test_capacity_failure_cleans_up_then_retries_and_saves_inventory(self):
        events = []
        def setup(*args):
            self.assertEqual(json.loads(self.state.read_text()), {'terraform_dir': str(self.directory)})
            events.append('setup')
            if len(events) == 1:
                self.advance(48 * 60)
                raise self.capacity_error()
            return self.success
        self.remote.setup_remote_environment.side_effect = setup
        self.cleanup.side_effect = lambda state: events.append('cleanup')
        compare_aws.provision(self.state, self.directory, 2)
        self.assertEqual(events, ['setup', 'cleanup', 'setup'])
        self.sleep.assert_called_once_with(60)
        saved = json.loads(self.state.read_text())
        self.assertEqual(saved['server_private_ip'], 'private-server')
        self.assertEqual(saved['client_public_ip'], 'public-client')
        args = self.remote.setup_remote_environment.call_args.args
        self.assertEqual(args[3], 'redisbloom-nightly-123-1-2')
        self.assertEqual(args[-1], 21600)
        self.assertIn('41.0 minutes remaining', self.console.getvalue())

    def test_non_capacity_errors_are_not_retried(self):
        error = TerraformCommandError(1, 'terraform apply', '', 'UnauthorizedOperation')
        self.remote.setup_remote_environment.side_effect = error
        with self.assertRaises(TerraformCommandError) as caught:
            compare_aws.provision(self.state, self.directory)
        self.assertIs(caught.exception, error)
        self.sleep.assert_not_called()
        self.cleanup.assert_not_called()
        self.assertTrue(self.state.exists())  # Final workflow teardown can still find it.

    def test_capacity_deadline_stops_retrying_after_cleanup(self):
        def setup(*args):
            self.advance(90 * 60)
            raise self.capacity_error()
        self.remote.setup_remote_environment.side_effect = setup
        with self.assertRaisesRegex(TimeoutError, '90-minute'):
            compare_aws.provision(self.state, self.directory)
        self.cleanup.assert_called_once_with(self.state)
        self.sleep.assert_not_called()
        self.assertEqual(self.remote.setup_remote_environment.call_count, 1)

    def test_cleanup_failure_aborts_retry(self):
        self.remote.setup_remote_environment.side_effect = self.capacity_error()
        self.cleanup.side_effect = RuntimeError('cleanup failed')
        with self.assertRaisesRegex(RuntimeError, 'cleanup failed'):
            compare_aws.provision(self.state, self.directory)
        self.assertEqual(self.remote.setup_remote_environment.call_count, 1)
        self.sleep.assert_not_called()

    def test_backoff_does_not_start_an_attempt_after_deadline(self):
        def setup(*args):
            self.advance(90 * 60 - 20)
            raise self.capacity_error()
        self.remote.setup_remote_environment.side_effect = setup
        with self.assertRaisesRegex(TimeoutError, '90-minute'):
            compare_aws.provision(self.state, self.directory)
        self.sleep.assert_called_once_with(20)
        self.assertEqual(self.remote.setup_remote_environment.call_count, 1)

    def test_success_does_not_retry(self):
        compare_aws.provision(self.state, self.directory)
        self.assertEqual(self.remote.setup_remote_environment.call_count, 1)
        self.sleep.assert_not_called()
        self.cleanup.assert_not_called()


if __name__ == '__main__':
    unittest.main()
