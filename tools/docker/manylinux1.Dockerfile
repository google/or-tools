FROM quay.io/pypa/manylinux1_x86_64:latest

ENV SRC_ROOT /root/src
ENV BUILD_ROOT /root/build
ENV EXPORT_ROOT /export
ENV SRC_GIT_URL https://github.com/google/or-tools
ENV SRC_GIT_BRANCH master
# The build of Python 2.6.x bindings is known to be broken.
ENV SKIP_PLATFORMS "cp26-cp26m cp26-cp26mu"

RUN yum -y update && yum -y install \
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
    zlib-devel

# WARNING
# We cannot use wget to download the needed packages due to a bug that leads
# to an incorrect checking of Server Alternate Name (SAN) property in the SSL
# certificate and makes wget to fail with something like:
#
# ERROR: certificate common name `*.kitware.com' doesn't match requested host name `cmake.org'.
#
# Note: 'wget --no-check-certificate' is not an option since we are building
#       distribution binaries.

RUN mkdir -p "$BUILD_ROOT"

# Update cmake, the system shipped version is too old for or-tools.
WORKDIR "$BUILD_ROOT"
RUN curl --location-trusted --remote-name https://cmake.org/files/v3.8/cmake-3.8.2.tar.gz -o cmake-3.8.2.tar.gz
RUN tar xzf cmake-3.8.2.tar.gz
WORKDIR cmake-3.8.2
RUN ./bootstrap --prefix=/usr
RUN make
RUN make install

# Update swig, the system shipped version doesn't support PY3.
WORKDIR "$BUILD_ROOT"
RUN curl --location-trusted --remote-name https://downloads.sourceforge.net/project/swig/swig/swig-3.0.12/swig-3.0.12.tar.gz -o swig-3.0.12.tar.gz
RUN tar xzf swig-3.0.12.tar.gz
WORKDIR swig-3.0.12
RUN ./configure --prefix=/usr
RUN make
RUN make install

RUN git clone -b "$SRC_GIT_BRANCH" --single-branch "$SRC_GIT_URL" "$SRC_ROOT"
WORKDIR "$SRC_ROOT"
RUN make third_party

COPY build-manylinux1.sh "$BUILD_ROOT"
RUN chmod ugo+x "${BUILD_ROOT}/build-manylinux1.sh"
