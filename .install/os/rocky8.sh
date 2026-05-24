#!/usr/bin/env bash
# Rocky Linux 8. Identical EL8 default install as AlmaLinux 8 (both are
# RHEL 8 rebuilds; powertools is the same upstream repo on both).

# shellcheck source=../lib/packages.sh
. "$LIB/packages.sh"

el8_default_install

# Install aws-cli for uploading artifacts to s3
curl "https://awscli.amazonaws.com/awscli-exe-linux-x86_64.zip" -o "awscliv2.zip"
unzip awscliv2.zip
./aws/install
