#!/bin/bash
set -x
set -e
docker build -t openroad/openroad --target base-dependencies .
docker run -u $(id -u ${USER}):$(id -g ${USER}) -v $(pwd):/OpenSTA openroad/openroad bash -c "./OpenSTA/jenkins/install.sh"
