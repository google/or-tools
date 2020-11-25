# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/r/opensuse/tumbleweed
FROM opensuse/tumbleweed AS base
LABEL maintainer="corentinl@google.com"
# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN zypper update -y \
&& zypper install -y git gcc gcc-c++ cmake \
 wget which lsb-release util-linux pkgconfig autoconf libtool zlib-devel \
&& zypper clean -a
ENV CC=gcc CXX=g++
CMD [ "/usr/bin/bash" ]

FROM base AS swig
RUN zypper update -y \
&& zypper install -y swig \
&& zypper clean -a
