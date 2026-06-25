#!/usr/bin/env bash
# Rocky Linux 9. Identical EL9 default install as AlmaLinux 9 (both are
# RHEL 9 rebuilds).

# shellcheck source=../lib/packages.sh
. "$LIB/packages.sh"

el9_default_install

# Install aws-cli for uploading artifacts to s3
install_aws_cli
