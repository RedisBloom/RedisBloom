#!/usr/bin/env bash
# Debian 13 (trixie). Default Debian install. Dockerfile.trixie runs
# install_cmake.sh after this script for a newer cmake.

# shellcheck source=../lib/sets.sh
. "$LIB/sets.sh"

debian_default_install
