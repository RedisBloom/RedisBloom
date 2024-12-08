#!/bin/bash
MODE=$1 # whether to install using sudo or not
set -e
export DEBIAN_FRONTEND=noninteractive
echo "::group::Install dependencies"
$MODE yum update -y
# Install the RPM package that provides the Software Collections (SCL) required for devtoolset-11
$MODE yum install -y https://vault.centos.org/centos/7/extras/x86_64/Packages/centos-release-scl-rh-2-3.el7.centos.noarch.rpm

# http://mirror.centos.org/centos/7/ is deprecated, so we changed the above link to `https://vault.centos.org`,
# and we have to change the baseurl in the repo file to the working mirror (from mirror.centos.org to vault.centos.org)
$MODE sed -i 's/mirrorlist=/#mirrorlist=/g' /etc/yum.repos.d/CentOS-SCLo-scl-rh.repo                        # Disable mirrorlist
$MODE sed -i 's/#baseurl=http:\/\/mirror/baseurl=http:\/\/vault/g' /etc/yum.repos.d/CentOS-SCLo-scl-rh.repo # Enable a working baseurl

$MODE yum install -y wget git which devtoolset-11-gcc devtoolset-11-gcc-c++ devtoolset-11-make \
  rsync python3 unzip tar python3-devel python3-pip awscli libffi-devel

source /opt/rh/devtoolset-11/enable

cp /opt/rh/devtoolset-11/enable /etc/profile.d/scl-devtoolset-11.sh

$MODE yum install -y curl
$MODE yum install -y openssl11 openssl11-devel
$MODE ln -s `which openssl11` /usr/bin/openssl

# Install clang
$MODE yum install -y clang
echo "::endgroup::"

make --version
git --version

echo "::group::install Python 3.9.6"
wget https://www.python.org/ftp/python/3.9.6/Python-3.9.6.tgz
tar -xvf Python-3.9.6.tgz
cd Python-3.9.6
./configure
make -j `nproc`
make altinstall
cd ..
rm /usr/bin/python3 && ln -s `which python3.9` /usr/bin/python3
python3 --version
echo "::endgroup::"

echo "::group::install awscli"
curl "https://awscli.amazonaws.com/awscli-exe-linux-x86_64.zip" -o "awscliv2.zip"
unzip awscliv2.zip
./aws/install
echo "::endgroup::"

echo "::group::install cmake"
version=3.25.1
filename=cmake-${version}-linux-x86_64.sh
wget https://github.com/Kitware/CMake/releases/download/v${version}/${filename}
chmod u+x ./${filename}
$MODE ./${filename} --skip-license --prefix=/usr/local --exclude-subdir
cmake --version
echo "::endgroup::"
