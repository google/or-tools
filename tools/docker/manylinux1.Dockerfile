FROM quay.io/pypa/manylinux1_x86_64:latest

RUN yum -y update && yum -y install \
    yum-utils \
    redhat-lsb \
    wget \
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
    which

WORKDIR /root
RUN wget --no-check-certificate https://cmake.org/files/v3.8/cmake-3.8.2.tar.gz
RUN tar xzf cmake-3.8.2.tar.gz
WORKDIR /root/cmake-3.8.2
RUN ./bootstrap --prefix=/usr
RUN make
RUN make install

WORKDIR /root
RUN wget --no-check-certificate https://downloads.sourceforge.net/project/swig/swig/swig-3.0.12/swig-3.0.12.tar.gz
RUN tar xzf swig-3.0.12.tar.gz
WORKDIR /root/swig-3.0.12
RUN ./configure --prefix=/usr
RUN make
RUN make install

ADD build-manylinux1.sh /root

WORKDIR /root
