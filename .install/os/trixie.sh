#!/usr/bin/env bash
# Debian 13 (trixie). Default Debian install. Dockerfile.trixie runs
# install_cmake.sh after this script for a newer cmake.

# shellcheck source=../lib/packages.sh
. "$LIB/packages.sh"

debian_default_install
