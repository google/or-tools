# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/ubuntu
FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt update -qq \
&& apt install -yq git wget build-essential lsb-release zlib1g-dev \
&& apt clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
ENTRYPOINT ["/bin/bash", "-c"]
CMD ["/bin/bash"]

# Install CMake 3.31.0
RUN ARCH=$(uname -m) \
&& wget -q "https://cmake.org/files/v3.31/cmake-3.31.0-linux-${ARCH}.sh" \
&& chmod a+x cmake-3.31.0-linux-${ARCH}.sh \
&& ./cmake-3.31.0-linux-${ARCH}.sh --prefix=/usr/local/ --skip-license \
&& rm cmake-3.31.0-linux-${ARCH}.sh

WORKDIR /root
ADD or-tools_amd64_ubuntu-20.04_cpp_v*.tar.gz .

RUN cd or-tools_*_v* && make test
