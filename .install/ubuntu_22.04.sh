#!/bin/bash
set -e
export DEBIAN_FRONTEND=noninteractive
MODE=$1 # whether to install using sudo or not

$MODE apt update -qq
$MODE apt upgrade -yqq
$MODE apt install -yqq git wget build-essential lcov openssl libssl-dev \
  unzip rsync libclang-dev clang curl autoconf automake libtool lcov valgrind
  # cmake python3 python3-pip python3-venv python3-dev 

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
