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
# Skip once cc already resolves to gcc-10 (idempotent re-runs / dry-run).
if [ "$(readlink -f /usr/bin/cc 2>/dev/null)" != /usr/bin/gcc-10 ]; then
    _run update-alternatives --install /usr/bin/cc  cc  /usr/bin/gcc-10 100
    _run update-alternatives --set     cc  /usr/bin/gcc-10
    _run update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-10 100
    _run update-alternatives --set     gcc /usr/bin/gcc-10
    _run update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-10 100
    _run update-alternatives --set     g++ /usr/bin/g++-10
fi
