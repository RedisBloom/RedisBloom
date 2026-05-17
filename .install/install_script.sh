#!/usr/bin/env bash
# Install build/test dependencies for RedisBloom.
#
# Flow:
#   1. detect canonical OSNICK (uname + /etc/os-release)
#   2. source lib/pm.sh — exports PM, SUDO, install helpers
#   3. source os/<osnick>.sh — installs OS packages and inlines any quirks
#   4. source lib/setup-python.sh — uv + venv + pip deps
#
# Same calling convention as the legacy script:
#   ./install_script.sh [sudo]    # "sudo" wraps installs (Linux); empty
#                                 # for macOS or already-root containers.

set -eu

MODE="${1:-}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
LIB="$HERE/lib"

# shellcheck source=lib/detect-osnick.sh
. "$LIB/detect-osnick.sh"
# shellcheck source=lib/pm.sh
. "$LIB/pm.sh"

OSNICK="$(detect_osnick)"
if [ -z "$OSNICK" ]; then
    echo "install_script.sh: cannot detect OSNICK (uname=$(uname -s))" >&2
    exit 1
fi

osfile="$HERE/os/$OSNICK.sh"
if [ ! -f "$osfile" ]; then
    echo "install_script.sh: unsupported OSNICK '$OSNICK' (no $osfile)" >&2
    echo "Supported: $(ls "$HERE/os" 2>/dev/null | sed 's/\.sh$//' | xargs)" >&2
    exit 1
fi

echo "==> [redisbloom] OSNICK=$OSNICK PM=$PM"

# shellcheck disable=SC1090
. "$osfile"

git config --global --add safe.directory '*' || true

# shellcheck source=lib/setup-python.sh
. "$LIB/setup-python.sh"

echo "==> [redisbloom] install_script.sh: done"
