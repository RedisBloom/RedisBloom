#!/usr/bin/env bash
# Debian 12 (bookworm). Default Debian install. Dockerfile.bookworm runs
# install_cmake.sh after this script for a newer cmake.

# shellcheck source=../lib/sets.sh
. "$LIB/sets.sh"

debian_default_install
