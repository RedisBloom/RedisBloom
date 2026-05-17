#!/usr/bin/env bash
# AlmaLinux 9. EL9 default install (RHEL_BASE + gcc-toolset-13). The
# Dockerfile then sets PATH to point at the toolset.

# shellcheck source=../lib/sets.sh
. "$LIB/sets.sh"

el9_default_install
