#!/usr/bin/env bash
# Rocky Linux 8. Identical EL8 default install as AlmaLinux 8 (both are
# RHEL 8 rebuilds; powertools is the same upstream repo on both).

# shellcheck source=../lib/packages.sh
. "$LIB/packages.sh"

el8_default_install

# Install aws-cli for uploading artifacts to s3
install_aws_cli
