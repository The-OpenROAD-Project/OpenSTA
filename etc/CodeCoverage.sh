#!/usr/bin/env bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2025, The OpenROAD Authors

# Generate an lcov/genhtml code coverage report for OpenSTA.
# Requires a coverage build: ./etc/Build.sh -coverage
#
# Usage:
#   ./etc/CodeCoverage.sh                Run all tests, generate report
#   ./etc/CodeCoverage.sh -tcl-only      Run only Tcl tests (skip C++ unit tests)

set -euo pipefail

cd "$(dirname $(readlink -f $0))/../"

buildDir="build"
reportDir="coverage-output"
tclOnly=no

_help() {
    cat <<EOF
usage: $0 [OPTIONS]

Generate an lcov/genhtml code coverage report for OpenSTA.
Requires a coverage build first: ./etc/Build.sh -coverage

OPTIONS:
  -tcl-only     Run only Tcl tests (skip C++ unit tests)
  -dir=PATH     Build directory. Default: ./build
  -help         Shows this message

OUTPUT:
  coverage-output/            HTML coverage report
  coverage-output/index.html  Main report page

EOF
    exit "${1:-1}"
}

while [ "$#" -gt 0 ]; do
    case "${1}" in
        -h|-help)
            _help 0
            ;;
        -tcl-only)
            tclOnly=yes
            ;;
        -dir=*)
            buildDir="${1#*=}"
            ;;
        -dir )
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

if [[ ! -d "${buildDir}" ]]; then
    echo "Build directory '${buildDir}' not found." >&2
    echo "Run ./etc/Build.sh -coverage first." >&2
    exit 1
fi

# Common lcov flags to suppress all known warnings:
#   mismatch - GCC/GoogleTest macro function range mismatches
#   gcov     - unexecuted blocks on non-branch lines (GCC optimizer artifacts)
#   source   - source file newer than .gcno notes file (stale build artifacts)
# Double-specification (X,X) suppresses the warning output entirely.
LCOV_IGNORE="--ignore-errors mismatch,mismatch,gcov,gcov,source,source,unused,unused,negative,negative"

# Clear stale coverage data before test execution.
# Old .gcda files from previous runs can cause gcov checksum mismatch noise.
find "${buildDir}" -name '*.gcda' -delete

# Step 1: Run tests
if [[ "${tclOnly}" == "yes" ]]; then
    echo "[INFO] Running Tcl tests only..."
    ctest --test-dir "${buildDir}" -L tcl -j $(nproc) --output-on-failure || true
else
    echo "[INFO] Running all tests..."
    ctest --test-dir "${buildDir}" -j $(nproc) --output-on-failure || true
fi

# Step 2: Capture coverage
echo "[INFO] Capturing coverage data..."
lcov --capture \
    -d "${buildDir}" \
    -o "${buildDir}/coverage.info" \
    --quiet \
    ${LCOV_IGNORE}

# Step 3: Filter system/build/test dirs
lcov --remove "${buildDir}/coverage.info" \
    '/usr/*' \
    '*/build/*' \
    '*/test/*' \
    -o "${buildDir}/filtered.info" \
    --quiet \
    ${LCOV_IGNORE}

# Step 4: Generate HTML report
GENHTML_IGNORE="--ignore-errors unmapped,unmapped,inconsistent,inconsistent,corrupt,corrupt,negative,negative,empty,empty,format,format,mismatch,mismatch,source,source"

mkdir -p "${reportDir}"
genhtml "${buildDir}/filtered.info" \
    --output-directory "${reportDir}" \
    --quiet \
    ${GENHTML_IGNORE} || true

echo ""
echo "=== Coverage report generated ==="
echo "Open $(pwd)/${reportDir}/index.html in a browser to view."
echo ""
lcov --summary "${buildDir}/filtered.info" ${LCOV_IGNORE} 2>&1 || true
