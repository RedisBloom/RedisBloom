"""Bound Terraform and its providers without waiting for inherited output pipes."""

import os
import signal
import subprocess
import sys


def run(command, timeout, grace=600):
    process = subprocess.Popen(command, start_new_session=True)
    try:
        try:
            code = process.wait(timeout=timeout)
            return code if code >= 0 else 128 - code
        except subprocess.TimeoutExpired:
            print('Terraform provisioning deadline reached; sending SIGINT', file=sys.stderr, flush=True)
            process.send_signal(signal.SIGINT)
            try:
                process.wait(timeout=grace)
            except subprocess.TimeoutExpired:
                print('Terraform grace period expired; killing its process group', file=sys.stderr, flush=True)
                return 137
            return 124
    finally:
        # A surviving provider can keep python_terraform.communicate() waiting on EOF.
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        process.wait(timeout=5)


if __name__ == '__main__':
    raise SystemExit(run(sys.argv[2:], float(sys.argv[1])))
