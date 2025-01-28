#!/bin/bash
set -e
export DEBIAN_FRONTEND=noninteractive
MODE=$1 # whether to install using sudo or not

$MODE apt update -qq
$MODE apt upgrade -yqq

add-apt-repository ppa:ubuntu-toolchain-r/test -y
apt update
apt-get install -yqq --fix-missing build-essential make autoconf automake libtool lcov git \
  wget zlib1g-dev lsb-release libssl-dev openssl ca-certificates curl unzip gcc-10 g++-10 \
  binfmt-support lsb-core awscli libclang-dev clang libffi-dev
update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-10 60 --slave /usr/bin/g++ g++ /usr/bin/g++-10

wget https://cmake.org/files/v3.28/cmake-3.28.0.tar.gz
tar -xzvf cmake-3.28.0.tar.gz
cd cmake-3.28.0
./configure
make -j `nproc`
make install
cd ..
ln -s /usr/local/bin/cmake /usr/bin/cmake
wget https://www.python.org/ftp/python/3.9.6/Python-3.9.6.tgz
tar -xvf Python-3.9.6.tgz
cd Python-3.9.6
./configure
make -j `nproc`
make altinstall
cd ..
rm /usr/bin/python3 && ln -s `which python3.9` /usr/bin/python3
rm /usr/bin/lsb_release
python3 --version
make --version
cmake --version
python3 -m pip install --upgrade pip
