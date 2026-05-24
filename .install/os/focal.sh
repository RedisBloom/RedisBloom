#!/usr/bin/env bash
# Ubuntu 20.04 (focal). Apt's gcc-9 on 20.04 misses C++20 features used
# downstream; pull gcc-10/g++-10 and switch /usr/bin/{gcc,g++} to them.

# shellcheck source=../lib/sets.sh
. "$LIB/sets.sh"

debian_default_install
apt_install gcc-10 g++-10
$SUDO update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-10 60 \
    --slave /usr/bin/g++ g++ /usr/bin/g++-10
