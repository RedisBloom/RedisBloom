#!/usr/bin/env bash
# Ubuntu 18.04 (bionic). Two distinct quirks beyond the Debian default:
#
#   1. Apt's default gcc is 7.x — too old for our C++. Pull gcc-10/g++-10
#      from the toolchain-r/test PPA and switch /usr/bin/{gcc,g++} to them.
#
#   2. Apt's cmake is 3.10 — too old for our CMakeLists. Build cmake 3.28
#      from source (~5 minutes on a typical CI runner) and symlink it as
#      /usr/bin/cmake so the older apt cmake is shadowed.
#
# We deliberately do *not* call debian_default_install before adding the PPA
# because software-properties-common (which provides add-apt-repository) is
# not in the base image and pulling it via apt_install lets us populate the
# apt cache in a single update pass.

# shellcheck source=../lib/sets.sh
. "$LIB/sets.sh"

apt_install software-properties-common lsb-core binfmt-support cargo zlib1g-dev
$SUDO add-apt-repository ppa:ubuntu-toolchain-r/test -y
debian_default_install
apt_install gcc-10 g++-10
$SUDO update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-10 60 \
    --slave /usr/bin/g++ g++ /usr/bin/g++-10

cd /tmp
wget -q https://cmake.org/files/v3.28/cmake-3.28.0.tar.gz
tar -xzf cmake-3.28.0.tar.gz
cd cmake-3.28.0
./configure
make -j"$(nproc)"
$SUDO make install
cd /
rm -rf /tmp/cmake-3.28.0 /tmp/cmake-3.28.0.tar.gz
$SUDO ln -sf /usr/local/bin/cmake /usr/bin/cmake
