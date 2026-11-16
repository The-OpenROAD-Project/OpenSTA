#!/usr/bin/env bash
# Stable workspace status consumed by Bazel when stamping (--config=release).
# Keys prefixed with STABLE_ rebuild stamped actions when they change.
# With --nostamp (the default), Bazel substitutes empty values.

STA_GIT_SHA1=$(git rev-parse HEAD 2>/dev/null || echo "unknown")
echo "STABLE_STA_GIT_SHA1 ${STA_GIT_SHA1}"
