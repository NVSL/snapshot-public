from ubuntu:20.04

maintainer Suyash Mahar, suyash12mahar@outlook.com

ARG tok

# Setup for ssh onto github
RUN mkdir -p /root/.ssh
RUN echo "Host github.com\n\tStrictHostKeyChecking no\n" >> /root/.ssh/config

run apt-get update
run apt install -y libvulkan-dev libisal-dev build-essential git python3 gcc-10 g++-10

ENV CXX=g++-10
ENV CC=gcc-10

RUN echo $tok
run git clone --recursive https://${tok}@github.com/NVSL/cxlbuf.git

WORKDIR /cxlbuf/vendor/spdk
RUN scripts/pkgdep.sh

run ./configure
WORKDIR /cxlbuf/
run make clean -C vendor/spdk
run ls -lah
run make -j10
