#!/usr/bin/env bash
# Debian 11 (bullseye). Default Debian install.

# shellcheck source=../lib/packages.sh
. "$LIB/packages.sh"

debian_default_install

# Bullseye's clang packaging registers clang as the highest-priority
# alternative for cc, which breaks the defer/errdefer macros (clang blocks
# need -fblocks) and mixes LTO formats at link time. Restore the DISTRO
# default (gcc-10 is bullseye's system gcc) — a fixed target, so every
# module's bootstrap converges on the same state regardless of order or of
# whatever newer side-by-side gcc another checkout installed.
$SUDO update-alternatives --install /usr/bin/cc  cc  /usr/bin/gcc-10 100
$SUDO update-alternatives --set     cc  /usr/bin/gcc-10
$SUDO update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-10 100
$SUDO update-alternatives --set     gcc /usr/bin/gcc-10
$SUDO update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-10 100
$SUDO update-alternatives --set     g++ /usr/bin/g++-10
