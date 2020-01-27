#!/bin/bash
set -x
set -e
mkdir -p /OpenSTA/build
cd /OpenSTA/build
cmake ..
make -j 4
