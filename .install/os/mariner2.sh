#!/usr/bin/env bash
# CBL-Mariner 2.0 (tdnf). Smaller repo set than dnf so we install
# best-effort and skip anything Mariner doesn't ship.

# shellcheck source=../lib/packages.sh
. "$LIB/packages.sh"

tdnf_default_install
git config --global --add safe.directory '*' || true

# Install aws-cli for uploading artifacts to s3
install_aws_cli
