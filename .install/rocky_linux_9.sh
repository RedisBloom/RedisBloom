#!/bin/bash
yum -y install epel-release
yum -y install gcc make cmake3 wget openssl-devel bzip2-devel libffi-devel zlib-devel wget scl-utils which gcc-toolset-13-gcc gcc-toolset-13-gcc-c++ gcc-toolset-13-libatomic-devel
yum groupinstall "Development Tools" -y
cp /opt/rh/gcc-toolset-13/enable /etc/profile.d/gcc-toolset-13.sh

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

# Detect architecture for AWS CLI download
ARCH=$(uname -m)
if [ "$ARCH" = "aarch64" ]; then
    AWS_ARCH="aarch64"
else
    AWS_ARCH="x86_64"
fi
curl "https://awscli.amazonaws.com/awscli-exe-linux-${AWS_ARCH}.zip" -o "awscliv2.zip"
unzip awscliv2.zip
./aws/install
