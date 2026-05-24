#!/usr/bin/env bash
# AlmaLinux 10. Base gcc on EL10 is recent enough; no toolset layering.
# Dockerfile.alma10 runs install_cmake.sh after this script for cmake 3.25.

# shellcheck source=../lib/sets.sh
. "$LIB/sets.sh"

rhel_default_install
