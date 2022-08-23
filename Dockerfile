FROM centos:centos7 AS base-dependencies
LABEL author="James Cherry"
LABEL maintainer="James Cherry <cherry@parallaxsw.com>"

# Install dev and runtime dependencies
RUN yum group install -y "Development Tools" \
    && yum install -y https://repo.ius.io/ius-release-el7.rpm \
    && yum install -y centos-release-scl \
    && yum install -y wget devtoolset-8 \
    && yum install -y qt5-qtbase-devel \
    devtoolset-8-libatomic-devel tcl-devel tcl tk libstdc++ tk-devel pcre-devel \
    python36u python36u-libs python36u-devel python36u-pip && \
    yum clean -y all && \
    rm -rf /var/lib/apt/lists/*

ENV CC=/opt/rh/devtoolset-8/root/usr/bin/gcc \
    CPP=/opt/rh/devtoolset-8/root/usr/bin/cpp \
    CXX=/opt/rh/devtoolset-8/root/usr/bin/g++ \
    PATH=/opt/rh/devtoolset-8/root/usr/bin:$PATH \
    LD_LIBRARY_PATH=/opt/rh/devtoolset-8/root/usr/lib64:/opt/rh/devtoolset-8/root/usr/lib:/opt/rh/devtoolset-8/root/usr/lib64/dyninst:/opt/rh/devtoolset-8/root/usr/lib/dyninst:/opt/rh/devtoolset-8/root/usr/lib64:/opt/rh/devtoolset-8/root/usr/lib:$LD_LIBRARY_PATH

# Install CMake
RUN wget https://cmake.org/files/v3.14/cmake-3.14.0-Linux-x86_64.sh && \
    chmod +x cmake-3.14.0-Linux-x86_64.sh  && \
    ./cmake-3.14.0-Linux-x86_64.sh --skip-license --prefix=/usr/local && rm -rf cmake-3.14.0-Linux-x86_64.sh \
    && yum clean -y all

# Install any git version > 2.6.5
RUN yum remove -y git* && yum install -y rh-git227
RUN rm -f /usr/bin/git; ln -s /opt/rh/rh-git227/root/bin/git /usr/bin/git

# Install SWIG
RUN yum remove -y swig \
    && wget https://github.com/swig/swig/archive/rel-4.0.1.tar.gz \
    && tar xfz rel-4.0.1.tar.gz \
    && rm -rf rel-4.0.1.tar.gz \
    && cd swig-rel-4.0.1 \
    && ./autogen.sh && ./configure --prefix=/usr && make -j $(nproc) && make install \
    && cd .. \
    && rm -rf swig-rel-4.0.1

# download CUDD
# RUN wget https://www.davidkebo.com/source/cudd_versions/cudd-3.0.0.tar.gz && \
#     tar -xvf cudd-3.0.0.tar.gz && \
#     rm cudd-3.0.0.tar.gz

# install CUDD
# RUN cd cudd-3.0.0 && \
#     mkdir ../cudd && \
#     ./configure --prefix=$HOME/cudd && \
#     make && \
#     make install


FROM base-dependencies AS builder

COPY . /OpenSTA
WORKDIR /OpenSTA

# Build
RUN mkdir build
#RUN cd buld && cmake .. -DCUDD=$HOME/cudd && make -j 8
RUN cd build && cmake .. && make -j 8

# Run sta on entry
ENTRYPOINT ["OpenSTA/app/sta"]
