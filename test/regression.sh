#!/usr/bin/env bash
# Regression test runner for OpenSTA module tests.
# Modeled after OpenROAD/test/regression_test.sh.
# Usage: regression.sh <sta_exe> <test_name>

STA_EXE="$1"
TEST_NAME="$2"

RESULT_DIR="results"
LOG_FILE="${RESULT_DIR}/${TEST_NAME}.log"
DIFF_FILE="${RESULT_DIR}/${TEST_NAME}.diff"

mkdir -p "${RESULT_DIR}"

# Run test, merge stderr into stdout, capture to log.
${STA_EXE} -no_init -no_splash -exit ${TEST_NAME}.tcl > ${LOG_FILE} 2>&1
sta_exit=$?

if [ $sta_exit -ne 0 ]; then
  echo "Error: sta exited with code ${sta_exit}"
  cat ${LOG_FILE}
  exit 1
fi

# Compare against golden file.
if [ ! -f "${TEST_NAME}.ok" ]; then
  echo "Error: golden file ${TEST_NAME}.ok not found"
  exit 1
fi

if diff ${TEST_NAME}.ok ${LOG_FILE} > ${DIFF_FILE} 2>&1; then
  exit 0
else
  echo "FAIL: output differs from ${TEST_NAME}.ok"
  cat ${DIFF_FILE}
  exit 1
fi
