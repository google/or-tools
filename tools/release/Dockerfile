FROM ubuntu:18.04 AS env

#############
##  SETUP  ##
#############
RUN apt update -qq \
&& apt install -yq \
 git pkg-config wget make autoconf libtool zlib1g-dev gawk g++ curl subversion \
 lsb-release gpg openssl \
&& apt clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Install CMake 3.19.7
RUN wget "https://cmake.org/files/v3.19/cmake-3.19.7-Linux-x86_64.sh" \
&& chmod a+x cmake-3.19.7-Linux-x86_64.sh \
&& ./cmake-3.19.7-Linux-x86_64.sh --prefix=/usr/local/ --skip-license \
&& rm cmake-3.19.7-Linux-x86_64.sh

# Swig Install
RUN apt-get update -qq \
&& apt-get install -yq swig \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Java install (openjdk-11)
RUN apt-get update -qq \
&& apt-get install -yq default-jdk maven \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
ENV JAVA_HOME=/usr/lib/jvm/default-java

# Dotnet Install
# see https://docs.microsoft.com/en-us/dotnet/core/install/linux-package-manager-ubuntu-1804
RUN apt-get update -qq \
&& apt-get install -yq apt-transport-https \
&& wget -q https://packages.microsoft.com/config/ubuntu/18.04/packages-microsoft-prod.deb \
&& dpkg -i packages-microsoft-prod.deb \
&& apt-get update -qq \
&& apt-get install -yq dotnet-sdk-3.1 \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
# Trigger first run experience by running arbitrary cmd
RUN dotnet --info

ENV TZ=America/Los_Angeles
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

################
##  OR-TOOLS  ##
################
FROM env AS devel
WORKDIR /root

# Download sources
# use ORTOOLS_GIT_SHA1 to modify the command
# i.e. avoid docker reusing the cache when new commit is pushed
ARG ORTOOLS_GIT_BRANCH
ENV ORTOOLS_GIT_BRANCH ${ORTOOLS_GIT_BRANCH:-master}
ARG ORTOOLS_GIT_SHA1
ENV ORTOOLS_GIT_SHA1 ${ORTOOLS_GIT_SHA1:-unknown}
RUN git clone -b "${ORTOOLS_GIT_BRANCH}" --single-branch https://github.com/google/or-tools \
&& echo "sha1: $(cd or-tools && git rev-parse --verify HEAD)" \
&& echo "expected sha1: ${ORTOOLS_GIT_SHA1}"

# Build delivery
FROM devel AS delivery
WORKDIR /root/or-tools

ARG ORTOOLS_TOKEN
ENV ORTOOLS_TOKEN ${ORTOOLS_TOKEN}
ARG ORTOOLS_DELIVERY
ENV ORTOOLS_DELIVERY ${ORTOOLS_DELIVERY:-all}
RUN ./tools/release/build_delivery_linux.sh "${ORTOOLS_DELIVERY}"
