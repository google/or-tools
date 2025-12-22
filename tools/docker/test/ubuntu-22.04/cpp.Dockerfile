# ref: https://hub.docker.com/_/ubuntu
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update -qq \
&& apt-get install -yq build-essential zlib1g-dev \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Install CMake 3.31.0
RUN ARCH=$(uname -m) \
&& wget -q "https://cmake.org/files/v3.31/cmake-3.31.0-linux-${ARCH}.sh" \
&& chmod a+x cmake-3.31.0-linux-${ARCH}.sh \
&& ./cmake-3.31.0-linux-${ARCH}.sh --prefix=/usr/local/ --skip-license \
&& rm cmake-3.31.0-linux-${ARCH}.sh

WORKDIR /root
ADD or-tools_amd64_ubuntu-22.04_cpp_v*.tar.gz .

RUN cd or-tools_*_v* && make test
