# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/centos
FROM centos:latest AS base
LABEL maintainer="corentinl@google.com"
# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN dnf -y update \
&& dnf -y install \
 git wget which redhat-lsb-core openssl-devel pkgconfig autoconf libtool zlib-devel \
&& dnf -y groupinstall "Development Tools" \
&& dnf clean all \
&& rm -rf /var/cache/dnf
# Install CMake 3.17.2
RUN wget "https://cmake.org/files/v3.17/cmake-3.17.2-Linux-x86_64.sh" \
&& chmod a+x cmake-3.17.2-Linux-x86_64.sh \
&& ./cmake-3.17.2-Linux-x86_64.sh --prefix=/usr/local/ --skip-license \
&& rm cmake-3.17.2-Linux-x86_64.sh
CMD [ "/usr/bin/bash" ]

FROM base AS swig
RUN dnf -y update \
&& dnf -y install pcre-devel \
&& dnf clean all \
&& rm -rf /var/cache/dnf \
&& wget "https://downloads.sourceforge.net/project/swig/swig/swig-4.0.1/swig-4.0.1.tar.gz" \
&& tar xvf swig-4.0.1.tar.gz \
&& rm swig-4.0.1.tar.gz \
&& cd swig-4.0.1 \
&& ./configure --prefix=/usr \
&& make -j 4 \
&& make install \
&& cd .. \
&& rm -rf swig-4.0.1
