# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/centos
FROM centos:latest AS base
LABEL maintainer="corentinl@google.com"
# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN yum -y update \
&& yum -y install git wget openssl-devel \
&& yum -y groupinstall "Development Tools" \
&& yum clean all \
&& rm -rf /var/cache/yum
# Install CMake 3.16.4
RUN wget "https://cmake.org/files/v3.16/cmake-3.16.4-Linux-x86_64.sh" \
&& chmod a+x cmake-3.16.4-Linux-x86_64.sh \
&& ./cmake-3.16.4-Linux-x86_64.sh --prefix=/usr/local/ --skip-license \
&& rm cmake-3.16.4-Linux-x86_64.sh
CMD [ "/usr/bin/bash" ]

FROM base AS swig
RUN yum -y update \
&& yum -y install pcre-devel \
&& yum clean all \
&& rm -rf /var/cache/yum \
&& wget "https://downloads.sourceforge.net/project/swig/swig/swig-4.0.1/swig-4.0.1.tar.gz" \
&& tar xvf swig-4.0.1.tar.gz \
&& rm swig-4.0.1.tar.gz \
&& cd swig-4.0.1 \
&& ./configure --prefix=/usr \
&& make -j 4 \
&& make install \
&& cd .. \
&& rm -rf swig-4.0.1
