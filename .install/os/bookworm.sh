#!/usr/bin/env bash
# Debian 12 (bookworm). Default Debian install. Dockerfile.bookworm runs
# install_cmake.sh after this script for a newer cmake.

# shellcheck source=../lib/packages.sh
. "$LIB/packages.sh"

debian_default_install
