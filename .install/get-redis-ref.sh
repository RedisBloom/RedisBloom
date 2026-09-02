#!/usr/bin/env bash
# Print the Redis git ref that redisbloom is built/tested against.
#
# The ref is derived from pack/ramp.yml, so the version lives in a single place
# and is not duplicated across Dockerfiles and CI workflows:
#   * `redis_ref`, when set, is used first (verbatim git ref)
#   * otherwise `compatible_redis_version` is used:
#       - the dev/unreleased placeholder 99.99 (or 99.99.99) -> "unstable" branch
#       - any real version (e.g. "8.8") is used as the git ref verbatim
# Values may be quoted or unquoted; an inline '#' comment, if any, is stripped.
#
# It lives under .install/ (not sbin/) so it is copied into the Docker build
# context together with the rest of .install/, letting install_redis.sh reuse
# the very same reader instead of re-implementing the parse.
#
# Usage:
#   .install/get-redis-ref.sh        # prints the ref, e.g. "unstable" or "8.8"
#
# In a GitHub Actions step:
#   echo "redis-ref=$(.install/get-redis-ref.sh)" >> "$GITHUB_OUTPUT"

set -euo pipefail

PROGNAME="${BASH_SOURCE[0]}"
HERE="$(cd "$(dirname "$PROGNAME")" &>/dev/null && pwd)"
ROOT="$(cd "$HERE/.." &>/dev/null && pwd)"
RAMP_FILE="$ROOT/pack/ramp.yml"

if [[ ! -f "$RAMP_FILE" ]]; then
	echo "Error: RAMP manifest not found at $RAMP_FILE" >&2
	exit 1
fi

# Value of a top-level RAMP key, with inline comment, surrounding whitespace
# and quotes stripped.
ramp_field() {
	local key="$1"
	sed -nE "s/^${key}:[[:space:]]*(.*)$/\1/p" "$RAMP_FILE" | head -n1 \
		| sed -E 's/[[:space:]]*#.*$//; s/^[[:space:]]+//; s/[[:space:]]+$//; s/^"(.*)"$/\1/; s/^'\''(.*)'\''$/\1/'
}

# The dev/unreleased placeholder (99.99 or 99.99.99) means we track the
# 'unstable' branch; a real version is used as the git ref directly.
map_redis_ref() {
	case "$1" in
		99.99 | 99.99.99) echo "unstable" ;;
		*)                echo "$1" ;;
	esac
}

REDIS_REF="$(ramp_field redis_ref)"
if [[ -z "$REDIS_REF" ]]; then
	COMPAT_VERSION="$(ramp_field compatible_redis_version)"
	if [[ -z "$COMPAT_VERSION" ]]; then
		echo "Error: neither 'redis_ref' nor 'compatible_redis_version' is defined in $RAMP_FILE" >&2
		exit 1
	fi
	REDIS_REF="$(map_redis_ref "$COMPAT_VERSION")"
else
	REDIS_REF="$(map_redis_ref "$REDIS_REF")"
fi

echo "$REDIS_REF"
