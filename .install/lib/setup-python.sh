#!/usr/bin/env bash
# Provision uv + a project-local venv + pip dependencies for RedisBloom.
#
# Sourced by install_script.sh after the OS package install. Reads $ROOT
# (set by install_script.sh) and $HERE (path to .install/). Writes
# $ROOT/venv/.
#
# Replaces the legacy all-in-one pip bootstrap script (now deleted): all pip
# work lives here so `make bootstrap` is just install_script.sh + done.
#
# Optional input: SETUP_PYTHON_VERSION (default: 3.12). Selects the interpreter
# uv uses for `$ROOT/venv`. Currently overridden to "3.11" by pm.sh's
# el8_default_install — EL8's base python3 is 3.6 (too old), and uv's auto-
# downloaded 3.12 makes psutil's wheel-less aarch64 source build look for
# Python.h against the wrong interpreter. Set this yourself before sourcing
# the script if you need a non-default interpreter on another OS.

# Required by callers — set by install_script.sh. Fail fast if absent rather
# than producing a confusing `uv venv ""` failure later.
: "${ROOT:?setup-python.sh: ROOT not set (must be sourced by install_script.sh)}"
: "${HERE:?setup-python.sh: HERE not set (must be sourced by install_script.sh)}"

if ! command -v uv >/dev/null 2>&1; then
    echo "==> [redisbloom] installing uv"
    curl -LsSf https://astral.sh/uv/install.sh | sh
    export PATH="$HOME/.local/bin:$HOME/.cargo/bin:$PATH"
fi

if ! command -v uv >/dev/null 2>&1; then
    echo "setup-python.sh: ERROR: uv installation failed; cannot create venv" >&2
    # Hard-fail: continuing past this point produces a "successful" bootstrap
    # with no venv on disk, which silently breaks every later test/flow step.
    return 1 2>/dev/null || exit 1
fi

# A stale or partial venv (e.g. a previous `make bootstrap` aborted halfway,
# or the developer ran `python3 -m venv` against a now-missing python) shows
# up as `$ROOT/venv` existing but `bin/python` not being executable. Wipe
# and recreate so we don't trip the executable check below.
if [ -d "$ROOT/venv" ] && [ ! -x "$ROOT/venv/bin/python" ]; then
    echo "==> [redisbloom] $ROOT/venv looks broken (no bin/python); recreating"
    rm -rf "$ROOT/venv"
fi

if [ ! -d "$ROOT/venv" ]; then
    uv venv "$ROOT/venv" --python "${SETUP_PYTHON_VERSION:-3.12}"
fi

if [ ! -x "$ROOT/venv/bin/python" ]; then
    echo "setup-python.sh: missing $ROOT/venv/bin/python (uv venv step failed?)" >&2
    exit 1
fi

# All pip work goes through `uv pip --python <venv>` (never --system, never
# under sudo). Sourcing under sudo would otherwise resolve uv against /usr's
# python3 (3.6 on EL8) and break rltest.
uv_pip() {
    uv pip install --python "$ROOT/venv/bin/python" "$@"
}

uv_pip --upgrade pip wheel "setuptools<81"
uv_pip -r "$HERE/build_package_requirements.txt"

# tests/flow/requirements.txt is committed at the repo root; absent only on
# unusual checkouts (e.g. Dockerfile build context that excluded tests/).
if [ -f "$ROOT/tests/flow/requirements.txt" ]; then
    (cd "$ROOT" && uv_pip -r tests/flow/requirements.txt)
fi
