#!/usr/bin/env bash
# Download completely before executing; never run a partial installer.
set -euo pipefail

installer=$(mktemp)
trap 'rm -f "$installer"' EXIT

for attempt in 1 2 3; do
    if curl -LsSf --connect-timeout 15 --max-time 60 \
        https://astral.sh/uv/install.sh -o "$installer"; then
        sh "$installer"
        exit 0
    else
        status=$?
    fi

    # DNS, connection, partial transfer, timeout, empty reply and receive errors.
    # HTTP, certificate and local filesystem failures are not retried.
    case "$status" in
        6|7|18|28|52|56) ;;
        *) exit "$status" ;;
    esac
    if [ "$attempt" -eq 3 ]; then
        exit "$status"
    fi
    echo "uv installer download failed (curl $status); retrying" >&2
    sleep "$((attempt * 2))"
done
