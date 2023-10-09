#!/bin/bash

REDIS_VERSION=7.2.1

cur_dir=$PWD
cd /tmp/

# Download redis source from github archive and extract it.
wget https://github.com/redis/redis/archive/${REDIS_VERSION}.tar.gz
tar -xvzf ${REDIS_VERSION}.tar.gz && cd redis-${REDIS_VERSION}

# Build redis from source and install it.
make && make install

cd $cur_dir
