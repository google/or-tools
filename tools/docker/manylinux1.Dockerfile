FROM quay.io/pypa/manylinux2010_x86_64:latest

RUN yum -y update \
&& yum -y install \
 autoconf \
 curl \
 gawk \
 gcc-c++ \
 git \
 libtool \
 make \
 patch \
 pcre-devel \
 redhat-lsb \
 subversion \
 which \
 zlib-devel \
 unzip zip \
&& yum clean all \
&& rm -rf /var/cache/yum

# Install CMake
# WARNING: We cannot use wget to download the needed packages due to a bug that leads
# to an incorrect checking of Server Alternate Name (SAN) property in the SSL
# certificate and makes wget to fail with something like:
# ERROR: certificate common name `*.kitware.com' doesn't match requested host name `cmake.org'.
# Note: 'wget --no-check-certificate' is not an option since we are building
# distribution binaries.
RUN curl --location-trusted \
 --remote-name https://cmake.org/files/v3.8/cmake-3.8.2.tar.gz \
 -o cmake-3.8.2.tar.gz \
&& tar xzf cmake-3.8.2.tar.gz \
&& rm cmake-3.8.2.tar.gz \
&& cd cmake-3.8.2 \
&& ./bootstrap --prefix=/usr \
&& make \
&& make install \
&& cd .. \
&& rm -rf cmake-3.8.2


# Install Swig
RUN curl --location-trusted \
 --remote-name "https://downloads.sourceforge.net/project/swig/swig/swig-3.0.12/swig-3.0.12.tar.gz" \
 -o swig-3.0.12.tar.gz \
&& tar xvf swig-3.0.12.tar.gz \
&& rm swig-3.0.12.tar.gz \
&& cd swig-3.0.12 \
&& ./configure --prefix=/usr \
&& make -j 4 \
&& make install \
&& cd .. \
&& rm -rf swig-3.0.12

# Update auditwheel to support manylinux2010
#RUN /opt/_internal/cpython-3.6.8/bin/pip install auditwheel==2.0.0

ENV TZ=America/Los_Angeles
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

################
##  OR-TOOLS  ##
################
ENV BUILD_ROOT /root/build
ENV SRC_GIT_URL https://github.com/google/or-tools
ENV SRC_ROOT /root/src
WORKDIR "$BUILD_ROOT"

ARG SRC_GIT_BRANCH
ENV SRC_GIT_BRANCH ${SRC_GIT_BRANCH:-master}
ARG SRC_GIT_SHA1
ENV SRC_GIT_SHA1 ${SRC_GIT_SHA1:-unknown}
RUN git clone -b "$SRC_GIT_BRANCH" --single-branch "$SRC_GIT_URL" "$SRC_ROOT"

WORKDIR "$SRC_ROOT"
RUN make third_party
RUN make cc

ENV EXPORT_ROOT /export
# The build of Python 2.6.x bindings is known to be broken.
# Python3.4 include conflict with abseil-cpp dynamic_annotation.h
ENV SKIP_PLATFORMS "cp26-cp26m cp26-cp26mu cp34-cp34m"

COPY build-manylinux1.sh "$BUILD_ROOT"
RUN chmod ugo+x "${BUILD_ROOT}/build-manylinux1.sh"
