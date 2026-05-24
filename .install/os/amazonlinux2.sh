#!/usr/bin/env bash
# Amazon Linux 2 — uses yum, ships gcc 7 and cmake 2.8 by default. Three
# distinct quirks before the standard yum install can succeed:
#
#   1. EPEL via amazon-linux-extras (provides things like jq).
#   2. CentOS Vault SCL repo + devtoolset-11 for a modern gcc/g++/make.
#      x86_64-only at upstream Vault; aarch64 hosts get a
#      "no devtoolset-11-* available" warning and continue (image is then
#      not usable for compilation but the abstract path still exercises).
#   3. cmake3 from EPEL, symlinked over the ancient base /usr/bin/cmake.
#      Anything depending on cmake>=3 (e.g. cpu_features) would otherwise
#      pick up the 2.8 binary first.

# shellcheck source=../lib/sets.sh
. "$LIB/sets.sh"

$SUDO amazon-linux-extras install epel -y
$SUDO yum -y install epel-release yum-utils
$SUDO yum-config-manager --add-repo http://vault.centos.org/centos/7/sclo/x86_64/rh/

yum_install autogen centos-release-scl scl-utils cmake3
$SUDO yum -y install --nogpgcheck --skip-broken \
    devtoolset-11-gcc devtoolset-11-gcc-c++ devtoolset-11-make || true

rhel_default_install
$SUDO ln -sf "$(command -v cmake3)" /usr/bin/cmake
