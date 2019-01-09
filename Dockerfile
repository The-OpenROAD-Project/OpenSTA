FROM ubuntu:18.04

RUN apt-get update && \
    apt-get install -y wget apt-utils git libtool autoconf

# download CUDD
RUN wget  https://www.davidkebo.com/source/cudd_versions/cudd-3.0.0.tar.gz && \
    tar -xvf cudd-3.0.0.tar.gz

# install main dependencies
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get install -y clang gcc tcl tcl-dev swig bison flex

# install CUDD
RUN cd cudd-3.0.0 && \
     ./configure --enable-dddmp --enable-obj --enable-shared --enable-static && \
    make && \
    make check

# clone and install OpenSTA
RUN git clone https://github.com/abdelrahmanhosny/OpenSTA.git && \
    cd OpenSTA && \
    libtoolize && \
    ./bootstrap && \
    ./configure && \
    make

# Run sta on entry
CMD OpenSTA/app/sta
