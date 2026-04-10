#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

CLION_CMAKE="/home/zshrout/.local/share/JetBrains/Toolbox/apps/clion/bin/cmake/linux/x64/bin/cmake"
if [[ -x "${CLION_CMAKE}" ]]; then
    CMAKE_BIN="${CLION_CMAKE}"
else
    CMAKE_BIN="cmake"
fi

BUILD_DIR="${REPO_ROOT}/build/linux-clang-debug-x11"
TARGETS=("$@")
if [[ ${#TARGETS[@]} -eq 0 ]]; then
    TARGETS=("CarrotSandbox")
fi

if [[ -d "/home/zshrout/vulkansdk/x86_64" ]]; then
    export VULKAN_SDK="/home/zshrout/vulkansdk/x86_64"
    export PATH="${VULKAN_SDK}/bin:${PATH}"
fi

CONFIGURE_ARGS=(
    -S "${REPO_ROOT}"
    -B "${BUILD_DIR}"
    -G Ninja
    -DCMAKE_BUILD_TYPE=Debug
    -DCMAKE_C_COMPILER=clang
    -DCMAKE_CXX_COMPILER=clang++
    -DCARROT_LINUX_WINDOWING=X11
)

if command -v clang-scan-deps-18 >/dev/null 2>&1; then
    CONFIGURE_ARGS+=(-DCMAKE_CXX_COMPILER_CLANG_SCAN_DEPS="$(command -v clang-scan-deps-18)")
fi

"${CMAKE_BIN}" "${CONFIGURE_ARGS[@]}"
"${CMAKE_BIN}" --build "${BUILD_DIR}" --target "${TARGETS[@]}" -j 8
