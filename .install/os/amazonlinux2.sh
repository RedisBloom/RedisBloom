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

# shellcheck source=../lib/packages.sh
. "$LIB/packages.sh"

# EPEL (jq, lcov, …) — enable only if epel-release isn't already installed.
rpm -q epel-release >/dev/null 2>&1 || _run amazon-linux-extras install epel -y
yum_install epel-release yum-utils
# autogen + cmake3 are arch-independent (base / EPEL, incl. aarch64).
yum_install autogen cmake3

# SCL / devtoolset-11 provide a modern gcc — but the CentOS Vault SCL repo is
# x86_64-only, so on aarch64 there is no devtoolset (the base gcc is used).
# Gate the whole SCL path on x86_64 so it isn't listed as a forever-unresolvable
# step on arm (where centos-release-scl / devtoolset-11 can never install).
if [ "$(uname -m)" = "x86_64" ]; then
    ls /etc/yum.repos.d/*sclo*.repo >/dev/null 2>&1 || _run yum-config-manager --add-repo http://vault.centos.org/centos/7/sclo/x86_64/rh/
    yum_install centos-release-scl scl-utils
    [ -d /opt/rh/devtoolset-11 ] || _run yum -y install --nogpgcheck --skip-broken \
        devtoolset-11-gcc devtoolset-11-gcc-c++ devtoolset-11-make || true
fi

rhel_default_install
# Point /usr/bin/cmake at cmake3 (EPEL) unless cmake is already >= 3. The path
# is resolved at run time, so print the literal $(command -v …) via _sh.
cmake --version 2>/dev/null | grep -qE 'version 3\.' || _sh 'sudo ln -sf "$(command -v cmake3)" /usr/bin/cmake'
# devtoolset present but not yet symlinked into /usr/local/bin — skip once done.
if [ -f /opt/rh/devtoolset-11/root/usr/bin/gcc ] && \
   [ "$(readlink -f /usr/local/bin/gcc 2>/dev/null)" != /opt/rh/devtoolset-11/root/usr/bin/gcc ]; then
    _run ln -sf /opt/rh/devtoolset-11/root/usr/bin/make /usr/local/bin/make
    _run ln -sf /opt/rh/devtoolset-11/root/usr/bin/gcc /usr/local/bin/gcc
    _run ln -sf /opt/rh/devtoolset-11/root/usr/bin/g++ /usr/local/bin/g++
    _run ln -sf /opt/rh/devtoolset-11/root/usr/bin/cc /usr/local/bin/cc
fi

# Install aws-cli for uploading artifacts to s3
install_aws_cli
