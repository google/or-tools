FROM minizinc/mznc2023:latest AS env

ENV SRC_GIT_BRANCH main

ENV TZ=America/Los_Angeles
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt-get update -yq \
&& apt-get -y install pkg-config git wget autoconf libtool zlib1g-dev gawk g++ \
 curl make lsb-release python-dev gfortran

# GCC 11
RUN apt update -qq \
&& apt install -yq software-properties-common \
&& add-apt-repository -y ppa:ubuntu-toolchain-r/test \
&& apt install -yq gcc-11 g++-11 \
&& apt clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
ENV CC=gcc-11 CXX=g++-11

# Install CMake 3.28.3
RUN wget -q "https://cmake.org/files/v3.28/cmake-3.28.3-linux-x86_64.sh" \
&& chmod a+x cmake-3.28.3-linux-x86_64.sh \
&& ./cmake-3.28.3-linux-x86_64.sh --prefix=/usr/local/ --skip-license \
&& rm cmake-3.28.3-linux-x86_64.sh

FROM env AS devel
WORKDIR /root
RUN git clone -b "$SRC_GIT_BRANCH" --single-branch https://github.com/google/or-tools

FROM devel AS build
WORKDIR /root/or-tools
RUN rm -rf build
RUN cmake -S. -Bbuild -DBUILD_PYTHON=OFF -DBUILD_JAVA=OFF -DBUILD_DOTNET=OFF -DUSE_SCIP=OFF -DUSE_COINOR=OFF -DBUILD_DEPS=ON
RUN cmake --build build --target all -j16

RUN ln -s /root/or-tools/build/bin/fzn-cp-sat /entry_data/fzn-exec

RUN cp /root/or-tools/ortools/flatzinc/mznlib/*mzn /entry_data/mzn-lib
