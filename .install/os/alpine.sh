#!/usr/bin/env bash
# Alpine Linux. ALPINE_BASE in lib/sets.sh already includes the musl-specific
# extras (musl-dev, linux-headers, gcompat, libstdc++, libgcc, bsd-compat-
# headers, ...) that used to live in a separate quirks file.
#
# The py3-* prebuilt packages avoid building those C extensions against musl
# from source during pip install; openblas-dev / xsimd back math/SIMD test
# deps.

# shellcheck source=../lib/sets.sh
. "$LIB/sets.sh"

alpine_default_install
apk_install py3-cryptography py3-numpy py3-psutil openblas-dev xsimd
