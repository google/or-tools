FROM quay.io/pypa/manylinux2010_x86_64:latest AS env

RUN yum -y update \
&& yum -y install \
 autoconf \
 curl wget \
 gawk \
 gcc-c++ \
 git \
 libtool \
 make \
 openssl-devel \
 patch \
 pcre-devel \
 redhat-lsb \
 subversion \
 which \
 zlib-devel \
 unzip zip \
&& yum clean all \
&& rm -rf /var/cache/yum

# Install CMake 3.18.5
RUN wget "https://cmake.org/files/v3.18/cmake-3.18.5-Linux-x86_64.sh" \
&& chmod a+x cmake-3.18.5-Linux-x86_64.sh \
&& ./cmake-3.18.5-Linux-x86_64.sh --prefix=/usr --skip-license \
&& rm cmake-3.18.5-Linux-x86_64.sh

# Install Swig 4.0.2
RUN curl --location-trusted \
 --remote-name "https://downloads.sourceforge.net/project/swig/swig/swig-4.0.2/swig-4.0.2.tar.gz" \
 -o swig-4.0.2.tar.gz \
&& tar xvf swig-4.0.2.tar.gz \
&& rm swig-4.0.2.tar.gz \
&& cd swig-4.0.2 \
&& ./configure --prefix=/usr \
&& make -j 4 \
&& make install \
&& cd .. \
&& rm -rf swig-4.0.2

ENV TZ=America/Los_Angeles
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

ENV BUILD_ROOT /root/build
ENV EXPORT_ROOT /export
# The build of Python 2.6.x bindings is known to be broken.
# Python3.4 include conflict with abseil-cpp dynamic_annotation.h
ENV SKIP_PLATFORMS "cp27-cp27m cp27-cp27mu cp34-cp34m cp35-cp35m"

COPY build-manylinux1.sh "$BUILD_ROOT/"
RUN chmod a+x "${BUILD_ROOT}/build-manylinux1.sh"

################
##  OR-TOOLS  ##
################
FROM env AS devel
ENV SRC_GIT_URL https://github.com/google/or-tools
ENV SRC_ROOT /root/src
WORKDIR "$BUILD_ROOT"

ARG SRC_GIT_BRANCH
ENV SRC_GIT_BRANCH ${SRC_GIT_BRANCH:-master}
ARG SRC_GIT_SHA1
ENV SRC_GIT_SHA1 ${SRC_GIT_SHA1:-unknown}
RUN git clone -b "$SRC_GIT_BRANCH" --single-branch "$SRC_GIT_URL" "$SRC_ROOT"

FROM devel AS third_party
WORKDIR "$SRC_ROOT"
RUN make third_party

FROM third_party as build
RUN make cc
