# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/fedora
FROM fedora:latest AS base
# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN dnf -y update \
&& dnf -y install git wget \
 which redhat-lsb-core pkgconfig autoconf libtool zlib-devel \
&& dnf -y install @development-tools \
&& dnf -y install gcc-c++ cmake \
&& dnf clean all
CMD [ "/usr/bin/bash" ]

FROM base AS swig
RUN dnf -y update \
&& dnf -y install swig \
&& dnf clean all
