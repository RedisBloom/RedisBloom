#!/bin/bash
MODE=$1 # whether to install using sudo or not
set -e
export DEBIAN_FRONTEND=noninteractive

amazon-linux-extras install epel -y
yum -y install epel-release yum-utils
yum-config-manager --add-repo http://vault.centos.org/centos/7/sclo/x86_64/rh/
yum -y install gcc make cmake3 git python-pip openssl-devel bzip2-devel libffi-devel zlib-devel wget centos-release-scl scl-utils which tar unzip
yum -y install devtoolset-11-gcc devtoolset-11-gcc-c++ devtoolset-11-make --nogpgcheck
. scl_source enable devtoolset-11 || true
make --version
git --version
wget https://www.python.org/ftp/python/3.9.6/Python-3.9.6.tgz
tar -xvf Python-3.9.6.tgz
cd Python-3.9.6
./configure
make -j `nproc`
make altinstall
cd ..
rm /usr/bin/python3 && ln -s `which python3.9` /usr/bin/python3
ln -s `which cmake3` /usr/bin/cmake
python3 --version
