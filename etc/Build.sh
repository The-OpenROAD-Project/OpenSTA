#!/usr/bin/env bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2025, The OpenROAD Authors

set -euo pipefail

DIR="$(dirname $(readlink -f $0))"
cd "$DIR/../"

# default values, can be overwritten by cmdline args
buildDir="build"
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
  numThreads=$(nproc --all)
elif [[ "$OSTYPE" == "darwin"* ]]; then
  numThreads=$(sysctl -n hw.ncpu)
else
  cat << EOF
WARNING: Unsupported OSTYPE: cannot determine number of host CPUs"
  Defaulting to 2 threads. Use -threads=N to use N threads"
EOF
  numThreads=2
fi
cmakeOptions=""
cleanBefore=no
compiler=gcc

_help() {
    cat <<EOF
usage: $0 [OPTIONS]

Build OpenSTA from source.

OPTIONS:
  -cmake='-<key>=<value> ...'   User defined cmake options
                                  Note: use single quote after
                                  -cmake= and double quotes if
                                  <key> has multiple <values>
                                  e.g.: -cmake='-DFLAGS="-a -b"'
  -compiler=COMPILER_NAME       Compiler name: gcc or clang
                                  Default: gcc
  -dir=PATH                     Path to store build files.
                                  Default: ./build
  -coverage                     Enable cmake coverage options
  -clean                        Remove build dir before compile
  -threads=NUM_THREADS          Number of threads to use during
                                  compile. Default: \`nproc\` on linux
                                  or \`sysctl -n hw.ncpu\` on macOS
  -O0                           Disable optimizations for accurate
                                  coverage measurement
  -help                         Shows this message

EOF
    exit "${1:-1}"
}

while [ "$#" -gt 0 ]; do
    case "${1}" in
        -h|-help)
            _help 0
            ;;
        -coverage )
            cmakeOptions+=" -DCMAKE_BUILD_TYPE=Debug"
            cmakeOptions+=" -DCMAKE_CXX_FLAGS='--coverage'"
            cmakeOptions+=" -DCMAKE_EXE_LINKER_FLAGS='--coverage'"
            ;;
        -O0 )
            cmakeOptions+=" -DCMAKE_CXX_FLAGS_DEBUG='-g -O0'"
            ;;
        -cmake=*)
            cmakeOptions+=" ${1#*=}"
            ;;
        -clean )
            cleanBefore=yes
            ;;
        -compiler=*)
            compiler="${1#*=}"
            ;;
        -dir=* )
            buildDir="${1#*=}"
            ;;
        -threads=* )
            numThreads="${1#*=}"
            ;;
        -compiler | -cmake | -dir | -threads )
            echo "${1} requires an argument" >&2
            _help
            ;;
        *)
            echo "unknown option: ${1}" >&2
            _help
            ;;
    esac
    shift 1
done

case "${compiler}" in
    "gcc" )
        export CC="$(command -v gcc)"
        export CXX="$(command -v g++)"
        ;;
    "clang" )
        export CC="$(command -v clang)"
        export CXX="$(command -v clang++)"
        ;;
    *)
        echo "Compiler $compiler not supported. Use gcc or clang." >&2
        _help 1
esac

if [[ -z "${CC}" || -z "${CXX}" ]]; then
    echo "Compiler $compiler not installed." >&2
    _help 1
fi

if [[ "${cleanBefore}" == "yes" ]]; then
    rm -rf "${buildDir}"
fi

mkdir -p "${buildDir}"

echo "[INFO] Compiler: ${compiler} (${CC})"
echo "[INFO] Build dir: ${buildDir}"
echo "[INFO] Using ${numThreads} threads."

eval cmake "${cmakeOptions}" -B "${buildDir}" .
eval time cmake --build "${buildDir}" -j "${numThreads}"
