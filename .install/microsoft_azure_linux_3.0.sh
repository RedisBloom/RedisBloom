#!/bin/bash
# This script automates the process of setting up a development environment for RedisBloom on a Microsoft Azure Linux virtual machine.

set -e

# Update and install dev tools needed for building and testing
tdnf -y update && \
tdnf install -y \
        git \
        wget \
        gcc \
        make \
        cmake \
        libffi-devel \
        openssl-devel \
        build-essential \
        zlib-devel \
        bzip2-devel \
        python3-devel \
        which \
        unzip \
        ca-certificates \
        python3-pip

# Install AWS CLI for uploading to S3
pip3 install awscli --upgrade

# Need python 3.9 for the tests, which is not the default
wget https://www.python.org/ftp/python/3.9.9/Python-3.9.9.tgz && \
    tar -xf Python-3.9.9.tgz && \
    cd Python-3.9.9 && \
    ./configure --enable-optimizations && \
    make -j $(nproc) && \
    make altinstall && \
    cd .. && \
    rm -rf Python-3.9.9 Python-3.9.9.tgz && \
    ln -sf /usr/local/bin/python3.9 /usr/local/bin/python3 && \
    ln -sf /usr/local/bin/pip3.9 /usr/local/bin/pip3

