#!/bin/bash
set -x
set -e
docker build -t openroad/opensta --target base-dependencies .
docker run -u $(id -u ${USER}):$(id -g ${USER}) -v $(pwd):/OpenSTA openroad/opensta bash -c "./OpenSTA/jenkins/install.sh"
