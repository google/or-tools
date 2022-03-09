FROM minizinc/mznc2021:latest AS env

ENV SRC_GIT_BRANCH master

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
ENTRYPOINT ["/bin/bash", "-c"]
CMD ["/bin/bash"]

# Install CMake 3.21.1
RUN wget -q "https://cmake.org/files/v3.21/cmake-3.21.1-linux-x86_64.sh" \
&& chmod a+x cmake-3.21.1-linux-x86_64.sh \
&& ./cmake-3.21.1-linux-x86_64.sh --prefix=/usr/local/ --skip-license \
&& rm cmake-3.21.1-linux-x86_64.sh

FROM env AS devel
WORKDIR /root
RUN git clone -b "$SRC_GIT_BRANCH" --single-branch https://github.com/google/or-tools

FROM devel AS third_party
# Build Third parties
WORKDIR /root/or-tools
RUN make detect \
&& make third_party BUILD_PYTHON=OFF BUILD_JAVA=OFF BUILD_DOTNET=OFF USE_SCIP=OFF USE_COINOR=OFF

# Build project
FROM third_party AS build
RUN make compile JOBS=8

RUN ln -s /root/or-tools/bin/fz /entry_data/fzn-exec

RUN cp /root/or-tools/ortools/flatzinc/mznlib/*mzn /entry_data/mzn-lib
