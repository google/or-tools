# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/ubuntu
FROM ubuntu:latest AS env

# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN apt-get update -qq \
&& DEBIAN_FRONTEND=noninteractive apt-get install -yq \
 git wget libssl-dev build-essential \
 cmake ninja-build python3 python3-venv pkgconf libglib2.0-dev \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

FROM env AS devel
WORKDIR /home/project
COPY . .

ENV PROJECT=or-tools
ARG TARGET
ENV TARGET=${TARGET:-unknown}

FROM devel AS toolchain
RUN ./tools/cross_compile.sh toolchain

FROM toolchain AS build
RUN cmake --version
RUN ./tools/cross_compile.sh build

FROM build AS test
RUN ./tools/cross_compile.sh qemu
RUN ./tools/cross_compile.sh test
