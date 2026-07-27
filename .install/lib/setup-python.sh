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

# list mode: record uv presence like any other dep, install nothing.
# uv installs to ~/.local/bin (or ~/.cargo/bin), which is not on PATH in the
# non-login bootstrap subshell — detect it there too, not just via PATH.
_have_uv() { command -v uv >/dev/null 2>&1 || [ -x "$HOME/.local/bin/uv" ] || [ -x "$HOME/.cargo/bin/uv" ]; }

# list / dry-run are read-only dependency reports — they must run in EVERY
# environment, so handle them BEFORE the ROOT/HERE asserts below (which only
# guard the real venv+pip work).
if [ "${CHECK_DEPS:-0}" = 1 ]; then
    # uv presence, routed through OPTIONAL_PKGS like any other dep.
    if _have_uv; then _uv=ok; else _uv=missing; fi
    if _is_optional uv; then
        [ "$_uv" = ok ] && DEPS_OPT_OK="$DEPS_OPT_OK uv" || DEPS_OPT_MISSING="$DEPS_OPT_MISSING uv"
    else
        [ "$_uv" = ok ] && DEPS_OK="$DEPS_OK uv" || DEPS_MISSING="$DEPS_MISSING uv"
    fi
    return 0 2>/dev/null || exit 0
fi

if [ "${DRY_RUN:-0}" = 1 ]; then
    # Print the exact uv + venv + pip sequence bootstrap would run (nothing is
    # executed) so dry-run output is a copy-pasteable script. Guarded on what's
    # already present so a provisioned host prints only the gaps: uv install
    # only if uv is missing; venv + pip only if the venv doesn't exist yet
    # (mirrors the real `[ ! -d venv ]` guard below).
    if [ ! -d "$ROOT/venv" ]; then
        _have_uv || _dry_line "curl -LsSf https://astral.sh/uv/install.sh | sh"
        # uv installs to ~/.local/bin (or ~/.cargo/bin), which may not be on
        # PATH — export it so the uv commands below resolve when pasted, exactly
        # as bootstrap does after installing uv.
        _dry_line 'export PATH="$HOME/.local/bin:$HOME/.cargo/bin:$PATH"'
        _dry_line "uv venv \"$ROOT/venv\" --python \"${SETUP_PYTHON_VERSION:-3.12}\""
        _dry_line "uv pip install --python \"$ROOT/venv/bin/python\" --upgrade pip wheel \"setuptools<81\""
        _dry_line "uv pip install --python \"$ROOT/venv/bin/python\" -r \"$HERE/build_package_requirements.txt\""
        [ -f "$ROOT/tests/flow/requirements.txt" ] && _dry_line "uv pip install --python \"$ROOT/venv/bin/python\" -r \"$ROOT/tests/flow/requirements.txt\""
    fi
    return 0 2>/dev/null || exit 0
fi

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
