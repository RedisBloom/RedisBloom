#!/usr/bin/env bash
# CBL-Mariner 2.0 (tdnf). Smaller repo set than dnf so we install
# best-effort and skip anything Mariner doesn't ship.

# shellcheck source=../lib/packages.sh
. "$LIB/packages.sh"

tdnf_default_install
git config --global --add safe.directory '*' || true

# Install aws-cli for uploading artifacts to s3
curl "https://awscli.amazonaws.com/awscli-exe-linux-x86_64.zip" -o "awscliv2.zip"
unzip awscliv2.zip
./aws/install
