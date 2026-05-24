#!/usr/bin/env bash
# Rocky Linux 9. Identical EL9 default install as AlmaLinux 9 (both are
# RHEL 9 rebuilds).

# shellcheck source=../lib/packages.sh
. "$LIB/packages.sh"

el9_default_install

# Install aws-cli for uploading artifacts to s3 (arch-aware: x86_64 / aarch64)
ARCH=$(uname -m)
if [[ $ARCH == "aarch64" ]]; then
    curl "https://awscli.amazonaws.com/awscli-exe-linux-aarch64.zip" -o "awscliv2.zip"
else
    curl "https://awscli.amazonaws.com/awscli-exe-linux-x86_64.zip" -o "awscliv2.zip"
fi
unzip awscliv2.zip
./aws/install
