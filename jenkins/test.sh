docker run -u $(id -u ${USER}):$(id -g ${USER}) -v $(pwd):/OpenSTA openroad/openroad bash -c "/OpenSTA/test/regression"
