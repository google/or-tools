FROM fedora:35
LABEL maintainer="corentinl@google.com"

RUN dnf -y update \
&& dnf -y install git \
 wget which redhat-lsb-core pkgconfig autoconf libtool zlib-devel \
&& dnf -y groupinstall "Development Tools" \
&& dnf -y install gcc-c++ cmake \
&& dnf clean all

WORKDIR /root
ADD or-tools_amd64_fedora-35_v*.tar.gz .

RUN cd or-tools_*_v* && make test_cc
