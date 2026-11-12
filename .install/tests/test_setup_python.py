"""Exercise bootstrap under nounset, including Bash 3.2 and legacy curl.

Run with BOOTSTRAP_BASH=/path/to/bash python3 .install/tests/test_setup_python.py.
All downloads and uv operations are stubbed; no network or host installs occur.
"""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "lib/setup-python.sh"


class BootstrapTests(unittest.TestCase):
    def run_case(self, modern, dry_run=False, failure=False):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            bindir = root / "bin"
            bindir.mkdir()
            (root / "home").mkdir()
            (root / "venv/bin").mkdir(parents=True)
            (root / "venv/bin/python").write_text("#!/bin/sh\nexit 0\n")
            (root / "venv/bin/python").chmod(0o755)
            # Dry-run only prints missing venv setup.
            if dry_run:
                (root / "venv/bin/python").unlink()
                (root / "venv/bin").rmdir()
                (root / "venv").rmdir()
            curl = bindir / "curl"
            curl.write_text('''#!/bin/sh
if [ "$1" = --retry-all-errors ]; then exit "$CURL_CAPABILITY"; fi
printf '%s\\n' "$@" > "$ROOT/curl-args"
while [ "$#" -gt 0 ]; do
    if [ "$1" = -o ]; then shift; output=$1; fi
    shift
done
printf '%s\\n' "$output" > "$ROOT/installer-path"
cat > "$output" <<'INSTALLER'
#!/bin/sh
touch "$ROOT/installer-ran"
printf '#!/bin/sh\\nexit 0\\n' > "$ROOT/bin/uv"
chmod +x "$ROOT/bin/uv"
INSTALLER
exit "$CURL_RESULT"
''')
            curl.chmod(0o755)
            env = dict(os.environ, ROOT=tmp, HERE=str(SCRIPT.parent.parent),
                       HOME=str(root / "home"), PATH=f"{bindir}:/usr/bin:/bin",
                       CURL_CAPABILITY="0" if modern else "2",
                       CURL_RESULT="22" if failure else "0",
                       DRY_RUN="1" if dry_run else "0", CHECK_DEPS="0")
            result = subprocess.run(
                [os.environ.get("BOOTSTRAP_BASH", "/bin/bash"), "-eu", "-c",
                 '_dry_line() { printf "%s\\n" "$1"; }; source "$1"',
                 "bootstrap-test", str(SCRIPT)],
                env=env, text=True, capture_output=True, timeout=10)
            self.assertEqual(result.returncode == 0, not failure, result.stderr)
            self.assertNotIn("unbound variable", result.stderr)
            if dry_run:
                self.assertEqual("--retry-all-errors" in result.stdout, modern)
                self.assertFalse((root / "curl-args").exists())
                self.assertIn("--proto '=https' --proto-redir '=https'", result.stdout)
            else:
                args = (root / "curl-args").read_text().splitlines()
                self.assertEqual("--retry-all-errors" in args, modern)
                self.assertNotIn("", args)
                self.assertEqual(args[args.index("--proto") + 1], "=https")
                self.assertEqual(args[args.index("--proto-redir") + 1], "=https")
                self.assertEqual((root / "installer-ran").exists(), not failure)
                self.assertFalse(Path((root / "installer-path").read_text().strip()).exists())

    def test_bootstrap(self):
        for modern in (False, True):
            for dry_run, failure in ((False, False), (False, True), (True, False)):
                with self.subTest(modern=modern, dry_run=dry_run, failure=failure):
                    self.run_case(modern, dry_run, failure)


if __name__ == "__main__":
    unittest.main()
