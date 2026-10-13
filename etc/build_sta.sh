#!/bin/sh
# OpenSTA, Static Timing Analyzer
# Copyright (c) 2026, Parallax Software, Inc.
#
# Configure and build sta so WriteCmdDocs.tcl can generate command docs.
# Used by Read the Docs, not the main CI test build.
# Usage: etc/build_sta.sh

set -eu

root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$root"

build_dir="${STA_DOCS_BUILD_DIR:-$root/build/docs-sta}"
cudd_dir="${STA_DOCS_CUDD_DIR:-$root/build/cudd}"
jobs=$(nproc 2>/dev/null || echo 2)
cudd_tar=/tmp/cudd-3.0.0.tar.gz
cudd_url=https://github.com/oscc-ip/artifact/releases/download/cudd-3.0.0/build.tar.gz

if [ ! -d "$cudd_dir" ]; then
  echo "Downloading CUDD 3.0.0..."
  mkdir -p "$cudd_dir"
  wget -nv -O "$cudd_tar" "$cudd_url"
  tar -xzf "$cudd_tar" -C "$cudd_dir"
  rm -f "$cudd_tar"
fi

set -- -S "$root" -B "$build_dir"
if command -v ninja >/dev/null 2>&1; then
  set -- "$@" -G Ninja
fi

echo "Configuring OpenSTA in $build_dir ..."
# Debug + -g0: skip LTO (CMakeLists enables it for Release) and debug info
# so Read the Docs stays under the community build-time limit.
cmake "$@" \
  -DCUDD_DIR="$cudd_dir" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS_DEBUG="-O0 -g0" \
  -DUSE_TCL_READLINE=OFF

echo "Building sta..."
cmake --build "$build_dir" --target sta -- -j "$jobs"
