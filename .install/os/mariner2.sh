#!/usr/bin/env bash
# CBL-Mariner 2.0 (tdnf). Smaller repo set than dnf so we install
# best-effort and skip anything Mariner doesn't ship.

# shellcheck source=../lib/sets.sh
. "$LIB/sets.sh"

tdnf_default_install
git config --global --add safe.directory '*' || true
