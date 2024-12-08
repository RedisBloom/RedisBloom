#!/bin/bash
set -e
export DEBIAN_FRONTEND=noninteractive
MODE=$1 # whether to install using sudo or not

$MODE apt update -qq
$MODE apt upgrade -yqq
$MODE apt-get install -y build-essential make cmake autoconf automake libtool lcov git wget zlib1g-dev lsb-release libssl-dev openssl ca-certificates python3 python3-pip python3-venv curl unzip rsync libclang-dev clang
