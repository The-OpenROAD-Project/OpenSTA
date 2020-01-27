docker run -u $(id -u ${USER}):$(id -g ${USER}) -v $(pwd):/OpenSTA openroad/opensta bash -c "/OpenSTA/test/regression"
