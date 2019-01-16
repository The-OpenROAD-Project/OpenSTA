FROM ubuntu:18.04

RUN apt-get update && \
    apt-get install -y wget apt-utils git

# download CUDD
RUN wget https://www.davidkebo.com/source/cudd_versions/cudd-3.0.0.tar.gz && \
    tar -xvf cudd-3.0.0.tar.gz

# install main dependencies
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get install -y cmake gcc tcl tcl-dev swig bison flex

# install CUDD
RUN cd cudd-3.0.0 && \
    mkdir ../cudd && \
    ./configure --prefix=$HOME/cudd && \
    make && \
    make install

# clone and install OpenSTA
RUN git clone https://github.com/abk-openroad/OpenSTA.git && \
    cd OpenSTA && \
    mkdir build && \
    cd build && \
    cmake .. -DCUDD=$HOME/cudd && \
    make

# Run sta on entry
CMD OpenSTA/app/sta
