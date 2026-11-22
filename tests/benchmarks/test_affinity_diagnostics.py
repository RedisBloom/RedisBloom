"""Run with unittest discover -s tests/benchmarks -p 'test_affinity_diagnostics.py'."""

from contextlib import redirect_stdout
import io
import itertools
import json
from pathlib import Path
import sys
import tempfile
from types import ModuleType
import unittest
from unittest.mock import Mock, patch

import configure_affinity as affinity
import compare_aws


class AffinityDiagnosticsTests(unittest.TestCase):
    def test_snapshot_preserves_counters_and_handles_missing_files(self):
        def read(path):
            if str(path) == '/proc/interrupts':
                return ' CPU0 CPU1\n 27: 12 34 PCI-MSI eth0\n 28: 99 99 other\n'
            raise FileNotFoundError(str(path))

        output = io.StringIO()
        with patch.object(Path, 'read_text', read), redirect_stdout(output):
            affinity.debug_irqs('before', {'27': 0})
        record = json.loads(output.getvalue().removeprefix('AFFINITY_DEBUG '))
        self.assertEqual(record['interrupts'], ['CPU0 CPU1', ' 27: 12 34 PCI-MSI eth0'])
        self.assertIn('unavailable:', record['irqs']['27']['effective_affinity_list'])

    def test_pending_irq_still_excludes_its_entire_physical_core(self):
        siblings = {cpu: {cpu % 4, cpu % 4 + 4} for cpu in range(8)}
        with patch.object(Path, 'read_text', return_value='5'), patch.object(affinity, 'debug_irqs'):
            selected, reserved, effective = affinity.select_isolated_cores({'27': 0}, set(range(8)), siblings, 2)
        self.assertEqual(selected, [2, 3])
        self.assertEqual(reserved, {2, 3, 6, 7})
        self.assertEqual(effective, {'27': '5'})

    def test_timeout_keeps_original_failure_and_logs_final_state(self):
        with patch.object(Path, 'read_text', return_value='1'), \
                patch.object(affinity.time, 'monotonic', side_effect=itertools.count(0, 6)), \
                patch.object(affinity, 'debug_irqs') as debug, \
                patch.object(affinity.subprocess, 'run', return_value=Mock(returncode=3)):
            with self.assertRaisesRegex(RuntimeError, 'Not enough IRQ-free physical cores'):
                affinity.select_isolated_cores({'27': 0}, {0, 1, 2}, {i: {i} for i in range(3)}, 2)
        self.assertEqual(debug.call_args.args[0], 'timeout')
        self.assertEqual(debug.call_args.kwargs['excluded'], [0, 1])

    def test_remote_logs_survive_success_and_failure_without_breaking_json(self):
        remote = ModuleType('redisbench_admin.utils.remote')
        line = 'AFFINITY_DEBUG {"event": "waiting"}\n'
        inventory = {'server_public_ip': 'test-host', 'user': 'test-user'}
        with tempfile.TemporaryDirectory() as directory, \
                patch.dict(sys.modules, {'redisbench_admin.utils.remote': remote}):
            for code in (0, 1):
                with self.subTest(code=code), redirect_stdout(io.StringIO()) as console:
                    remote.execute_remote_commands = Mock(return_value=[
                        (code, [line[:8], line[8:], '{"cpulist": "2,3"}\n'], ['failure'] if code else [])])
                    log = Path(directory) / f'{code}.log'
                    if code:
                        with self.assertRaisesRegex(RuntimeError, 'Remote command failed'):
                            compare_aws.remote_commands(inventory, 'key', ['command'], diagnostics=log)
                    else:
                        result, = compare_aws.remote_commands(inventory, 'key', ['command'], diagnostics=log)
                        self.assertEqual(json.loads(result), {'cpulist': '2,3'})
                    self.assertEqual(log.read_text(), line)
                    self.assertIn('server_public_ip: AFFINITY_DEBUG', console.getvalue())


if __name__ == '__main__':
    unittest.main()
