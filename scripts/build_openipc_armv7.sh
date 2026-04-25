#!/bin/sh
set -eu

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
PROJECT_ROOT="$(CDPATH= cd -- "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${PROJECT_ROOT}/build-armv7}"
TOOLCHAIN_FILE="${TOOLCHAIN_FILE:-${PROJECT_ROOT}/cmake/toolchains/openipc-armv7-linux-gnueabihf.cmake}"

find_compiler_prefix() {
    if [ -n "${OPENIPC_ARM_GCC_PREFIX:-}" ]; then
        printf '%s' "${OPENIPC_ARM_GCC_PREFIX}"
        return 0
    fi

    for prefix in \
        /opt/homebrew/bin/armv7-unknown-linux-gnueabihf- \
        /usr/local/bin/armv7-unknown-linux-gnueabihf- \
        /opt/homebrew/bin/arm-linux-gnueabihf- \
        /usr/local/bin/arm-linux-gnueabihf- \
        /opt/homebrew/bin/arm-unknown-linux-gnueabihf- \
        /usr/local/bin/arm-unknown-linux-gnueabihf- \
        armv7-unknown-linux-gnueabihf- \
        arm-linux-gnueabihf- \
        arm-unknown-linux-gnueabihf- \
        arm-none-linux-gnueabihf-
    do
        if command -v "${prefix}g++" >/dev/null 2>&1; then
            printf '%s' "${prefix}"
            return 0
        fi
    done

    return 1
}

PREFIX="$(find_compiler_prefix || true)"
if [ -z "${PREFIX}" ]; then
    echo "No ARMv7 Linux cross-compiler found." >&2
    echo "Install one such as:" >&2
    echo "  brew tap messense/macos-cross-toolchains" >&2
    echo "  brew install messense/macos-cross-toolchains/armv7-unknown-linux-gnueabihf" >&2
    echo "Or provide your own prefix via OPENIPC_ARM_GCC_PREFIX=/path/to/<triplet>-" >&2
    exit 1
fi

export OPENIPC_ARM_GCC_PREFIX="${PREFIX}"

echo "Using cross-compiler prefix: ${OPENIPC_ARM_GCC_PREFIX}"
cmake -S "${PROJECT_ROOT}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}"
cmake --build "${BUILD_DIR}" -j

echo "Built ARMv7 binary:"
echo "  ${BUILD_DIR}/openipc_ip_bridge"
