# ref: https://hub.docker.com/_/ubuntu
FROM ubuntu:18.04

RUN apt-get update \
&& apt-get install -yq wget build-essential zlib1g-dev \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# GCC 11
RUN apt update -qq \
&& apt install -yq software-properties-common \
&& add-apt-repository -y ppa:ubuntu-toolchain-r/test \
&& apt install -yq gcc-11 g++-11 \
&& apt clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
ENV CC=gcc-11 CXX=g++-11
ENTRYPOINT ["/bin/bash", "-c"]
CMD ["/bin/bash"]

# Install CMake 3.21.1
RUN ARCH=$(uname -m) \
&& wget -q "https://cmake.org/files/v3.21/cmake-3.21.1-linux-${ARCH}.sh" \
&& chmod a+x cmake-3.21.1-linux-${ARCH}.sh \
&& ./cmake-3.21.1-linux-${ARCH}.sh --prefix=/usr/local/ --skip-license \
&& rm cmake-3.21.1-linux-${ARCH}.sh

#ENV TZ=America/Los_Angeles
#RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

WORKDIR /root
ADD or-tools_amd64_ubuntu-18.04_cpp_v*.tar.gz .

RUN cd or-tools_*_v* && make test
