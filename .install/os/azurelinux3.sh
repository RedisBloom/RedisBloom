#!/usr/bin/env bash
# Azure Linux 3 (tdnf). Same shape as mariner2 — Microsoft's distro lineage,
# tdnf-based, ships a smaller package set than dnf.

# shellcheck source=../lib/sets.sh
. "$LIB/sets.sh"

tdnf_default_install
git config --global --add safe.directory '*' || true
