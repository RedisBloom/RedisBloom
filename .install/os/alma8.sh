#!/usr/bin/env bash
# AlmaLinux 8. EL8 default install (RHEL_BASE + EPEL + powertools/crb +
# gcc-toolset-11). The Dockerfile then sets PATH to point at the toolset.

# shellcheck source=../lib/sets.sh
. "$LIB/sets.sh"

el8_default_install
