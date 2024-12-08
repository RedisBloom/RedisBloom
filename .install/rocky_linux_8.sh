#!/bin/bash
MODE=$1 # whether to install using sudo or not
set -e

$MODE dnf update -y

# Development Tools includes python11 and config-manager
$MODE dnf groupinstall "Development Tools" -yqq
# install pip
$MODE dnf install python3.11-pip -y

# powertools is needed to install epel
$MODE dnf config-manager --set-enabled powertools

# get epel to install gcc11
$MODE dnf install epel-release -yqq


yum -y install epel-release
yum -y install http://opensource.wandisco.com/centos/7/git/x86_64/wandisco-git-release-7-2.noarch.rpm
yum -y install gcc make cmake3 git openssl-devel bzip2-devel libffi-devel zlib-devel wget scl-utils gcc-toolset-13 which
yum groupinstall "Development Tools" -y
. scl_source enable gcc-toolset-13 || true
make --version
gcc --version
git --version
wget https://www.python.org/ftp/python/3.9.6/Python-3.9.6.tgz
tar -xvf Python-3.9.6.tgz
cd Python-3.9.6
./configure
make -j `nproc`
make altinstall
cd ..
rm /usr/bin/python3 && ln -s `which python3.9` /usr/bin/python3
cmake --version
python3 --version
