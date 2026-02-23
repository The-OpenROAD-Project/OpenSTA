#!/bin/bash
# Usage: ./make_coverage_report.sh [-B] [--tcl_only] [--O0]
# Generates an lcov/genhtml coverage report for OpenSTA tests.
#   --O0       Disable optimizations for accurate coverage (no inlining)
#   --tcl_only Run only Tcl tests (skip C++ unit tests)

set -euo pipefail

usage() {
  echo "Usage: $0 [OPTIONS]"
  echo ""
  echo "Build OpenSTA with coverage instrumentation, run tests, and generate"
  echo "an lcov/genhtml coverage report."
  echo ""
  echo "Options:"
  echo "  -B          Clean rebuild (remove build directory before building)"
  echo "  --O0        Disable optimizations (-O0) for accurate coverage measurement"
  echo "  --tcl_only  Run only Tcl tests (skip C++ unit tests)"
  echo "  -h, --help  Show this help message"
  echo ""
  echo "Output:"
  echo "  build/                 Build directory (sta executable at build/sta)"
  echo "  build/coverage_report/ HTML coverage report"
  exit 0
}

CLEAN_BUILD=0
TCL_ONLY=0
OPT_LEVEL=""
for arg in "$@"; do
  case "$arg" in
    -h|--help) usage ;;
    -B) CLEAN_BUILD=1 ;;
    --tcl_only) TCL_ONLY=1 ;;
    --O0) OPT_LEVEL="-O0" ;;
    *) echo "Unknown argument: $arg"; echo "Run '$0 --help' for usage."; exit 1 ;;
  esac
done

BUILD_DIR="build"
REPORT_DIR="build/coverage_report"

# Common lcov flags to suppress all known warnings:
#   mismatch - GCC/GoogleTest macro function range mismatches
#   gcov     - unexecuted blocks on non-branch lines (GCC optimizer artifacts)
#   source   - source file newer than .gcno notes file (stale build artifacts)
# Double-specification (X,X) suppresses the warning output entirely.
LCOV_IGNORE="--ignore-errors mismatch,mismatch,gcov,gcov,source,source"

echo "=== OpenSTA Coverage Report ==="
echo "Build directory: $BUILD_DIR"
echo "Report directory: $REPORT_DIR"
echo "Clean build: $CLEAN_BUILD"
echo "TCL only: $TCL_ONLY"
echo "Optimization: ${OPT_LEVEL:-default}"

# Step 1: Configure
if [ "$CLEAN_BUILD" -eq 1 ] && [ -d "$BUILD_DIR" ]; then
  echo "Removing $BUILD_DIR for clean rebuild..."
  rm -rf "$BUILD_DIR"
fi
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake .. \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_CXX_FLAGS="--coverage $OPT_LEVEL" \
  -DCMAKE_EXE_LINKER_FLAGS="--coverage" \
  -DBUILD_TESTS=ON

# Step 2: Build
make -j$(nproc)

# Step 2.5: Clear stale coverage data before test execution.
# Old .gcda files from previous builds can cause gcov checksum mismatch noise
# that pollutes test logs and leads to false regression diffs.
find . -name '*.gcda' -delete

# Step 3: Run tests
if [ "$TCL_ONLY" -eq 1 ]; then
  echo "Running Tcl tests only..."
  ctest -L tcl -j$(nproc) --output-on-failure || true
else
  echo "Running all tests..."
  ctest -j$(nproc) --output-on-failure || true
fi

# Step 4: Capture coverage
lcov --capture -d . -o coverage.info --quiet $LCOV_IGNORE

# Step 5: Filter system/build/test dirs
lcov --remove coverage.info '/usr/*' '*/build/*' '*/test/*' -o filtered.info --quiet $LCOV_IGNORE

# Step 6: Generate HTML report
ABS_REPORT_DIR="$(cd .. && pwd)/$REPORT_DIR"
genhtml filtered.info --output-directory "$ABS_REPORT_DIR" --quiet \
  --ignore-errors unmapped,unmapped,inconsistent,inconsistent,corrupt,corrupt,negative,negative,empty,empty,format,format,mismatch,mismatch,source,source || true

echo ""
echo "=== Coverage report generated ==="
echo "Open $ABS_REPORT_DIR/index.html in a browser to view."
echo "sta executable: $(pwd)/sta"
echo ""
lcov --summary filtered.info $LCOV_IGNORE 2>&1 || true
