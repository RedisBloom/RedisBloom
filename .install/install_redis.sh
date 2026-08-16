#!/usr/bin/env bash
set -e

if [ -z "${REDIS_REF}" ]; then
    echo "Error: REDIS_REF environment variable is required"
    exit 1
fi

echo "Installing Redis from ref: ${REDIS_REF}"

if [ -n "${SANITIZER}" ]; then
    echo "Building Redis with SANITIZER=${SANITIZER}"
fi

git clone https://github.com/redis/redis.git 
cd redis
git fetch origin ${REDIS_REF}
git checkout ${REDIS_REF}
git submodule update --init --recursive
MAKE_ARGS=()
if [ -n "${SANITIZER}" ]; then
    # The module is instrumented by clang (readies clang-sanitizer.defs) and a clang-built
    # shared object links no ASan runtime of its own, so redis must be instrumented by clang
    # too -- against gcc's libasan the module reports bogus global-buffer-overflows in RM_GetApi.
    MAKE_ARGS+=(CC=clang LD=clang)
    if grep -q '^ifdef SANITIZER' src/Makefile; then
        MAKE_ARGS+=("SANITIZER=${SANITIZER}")
    else
        # Redis grew Makefile SANITIZER support in 7.0; older refs ignore it silently and
        # produce an uninstrumented server the module cannot even dlopen. Pass the equivalent
        # flags by hand, mirroring what redis 7.x does (it also forces libc malloc).
        MAKE_ARGS+=(MALLOC=libc)
        MAKE_ARGS+=("REDIS_CFLAGS=-fsanitize=${SANITIZER} -fno-sanitize-recover=all -fno-omit-frame-pointer")
        MAKE_ARGS+=("REDIS_LDFLAGS=-fsanitize=${SANITIZER}")
    fi
fi

make "${MAKE_ARGS[@]}" -j$(nproc)
make install "${MAKE_ARGS[@]}"
cd ..

echo "Redis installed successfully"
redis-server --version
