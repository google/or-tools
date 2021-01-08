# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/ubuntu
FROM ubuntu:rolling AS env
LABEL maintainer="corentinl@google.com"
# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN apt-get update -qq \
&& apt-get install -yq git wget libssl-dev build-essential \
 ninja-build python3 pkgconf libglib2.0-dev \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Install CMake 3.19.2
RUN wget "https://cmake.org/files/v3.19/cmake-3.19.2-Linux-x86_64.sh" \
&& chmod a+x cmake-3.19.2-Linux-x86_64.sh \
&& ./cmake-3.19.2-Linux-x86_64.sh --prefix=/usr/local/ --skip-license \
&& rm cmake-3.19.2-Linux-x86_64.sh

FROM env AS devel
WORKDIR /home/project
COPY . .

FROM devel AS build
ENV PROJECT=glop
ENV TARGET=aarch64-linux-gnu
RUN ./tools/cross_compile.sh build

FROM build AS test
RUN ./tools/cross_compile.sh qemu
RUN ./tools/cross_compile.sh test
