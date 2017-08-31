FROM quay.io/pypa/manylinux1_x86_64:latest

RUN yum -y update && yum -y install \
    yum-utils \
    redhat-lsb \
    git \
    autoconf \
    libtool \
    zlib-devel \
    gawk \
    gcc-c++ \
    curl \
    subversion \
    make \
    pcre-devel \
    which \
    curl

# WARNING
# We cannot use wget to download the following packages due to a bug that leads
# to an incorrect checking of Server Alternate Name (SAN) property in the SSL
# certificate and makes wget to fail with something like:
#
# ERROR: certificate common name `*.kitware.com' doesn't match requested host name `cmake.org'.
#
# Note: 'wget --no-check-certificate' is not an option since we are building
#       distribution binaries.

WORKDIR /root
RUN curl --location-trusted --remote-name https://cmake.org/files/v3.8/cmake-3.8.2.tar.gz
RUN tar xzf cmake-3.8.2.tar.gz
WORKDIR /root/cmake-3.8.2
RUN ./bootstrap --prefix=/usr
RUN make
RUN make install

WORKDIR /root
RUN curl --location-trusted --remote-name https://downloads.sourceforge.net/project/swig/swig/swig-3.0.12/swig-3.0.12.tar.gz
RUN tar xzf swig-3.0.12.tar.gz
WORKDIR /root/swig-3.0.12
RUN ./configure --prefix=/usr
RUN make
RUN make install

RUN git clone https://github.com/google/or-tools /root/or-tools
WORKDIR /root/or-tools
RUN make third_party

ADD build-manylinux1.sh /root

WORKDIR /root
