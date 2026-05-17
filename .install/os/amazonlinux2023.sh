#!/usr/bin/env bash
# Amazon Linux 2023. Standard dnf-based RHEL flow; gcc/cmake on AL2023 are
# new enough that no toolset layering is needed.

# shellcheck source=../lib/sets.sh
. "$LIB/sets.sh"

rhel_default_install
