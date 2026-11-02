"""Bounded installer download tests; no network or real uv installation."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / 'lib' / 'install-uv.sh'

class InstallUvTests(unittest.TestCase):
    def run_case(self, codes, expected_status, expected_calls, installed=False, installer_status=0):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'curl').write_text('''#!/usr/bin/env python3
import json, os, sys
from pathlib import Path
root = Path(os.environ['PROBE_ROOT'])
count = root / 'count'
n = int(count.read_text()) if count.exists() else 0
count.write_text(str(n + 1))
args = sys.argv[1:]
assert args[:5] == ['-LsSf', '--connect-timeout', '15', '--max-time', '60']
output = Path(args[args.index('-o') + 1])
(root / 'output').write_text(str(output))
output.write_text('echo executed >> "$PROBE_ROOT/executed"\\nexit ' + os.environ['INSTALLER_STATUS'] + '\\n')
codes = json.loads(os.environ['CODES'])
sys.exit(codes[min(n, len(codes)-1)])
''')
            (root / 'sleep').write_text('#!/bin/sh\necho "$1" >> "$PROBE_ROOT/sleeps"\n')
            for name in ('curl', 'sleep'):
                (root / name).chmod(0o755)
            env = dict(os.environ, PATH=str(root) + os.pathsep + os.environ['PATH'],
                       PROBE_ROOT=str(root), CODES=json.dumps(codes), INSTALLER_STATUS=str(installer_status))
            result = subprocess.run(['bash', str(SCRIPT)], env=env, capture_output=True, timeout=5)
            self.assertEqual(result.returncode, expected_status, result.stderr)
            self.assertEqual(int((root / 'count').read_text()), expected_calls)
            self.assertEqual((root / 'executed').exists(), installed)
            if installed:
                self.assertEqual((root / 'executed').read_text(), 'executed\n')
            self.assertFalse(Path((root / 'output').read_text()).exists())
            sleeps = (root / 'sleeps').read_text().splitlines() if (root / 'sleeps').exists() else []
            self.assertEqual(sleeps, ['2', '4'][:expected_calls - 1])

    def test_success(self):
        self.run_case([0], 0, 1, installed=True)

    def test_transient_recovery_does_not_execute_partial_download(self):
        for code in (6, 7, 18, 28, 52, 56):
            with self.subTest(code=code):
                self.run_case([code, 0], 0, 2, installed=True)

    def test_retry_exhaustion(self):
        self.run_case([6], 6, 3)

    def test_permanent_errors_do_not_retry_or_execute(self):
        for code in (22, 23, 60):
            with self.subTest(code=code):
                self.run_case([code], code, 1)

    def test_installer_failure_is_not_retried(self):
        self.run_case([0], 42, 1, installed=True, installer_status=42)

if __name__ == '__main__':
    unittest.main()
