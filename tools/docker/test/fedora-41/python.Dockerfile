# ref: https://hub.docker.com/_/fedora
FROM fedora:41

RUN dnf -y update \
&& dnf -y install git \
 wget which redhat-lsb-core pkgconfig autoconf libtool zlib-devel \
&& dnf -y group install "Development Tools" \
&& dnf -y install gcc-c++ cmake \
&& dnf clean all

WORKDIR /root
ADD or-tools_amd64_fedora-41_python_v*.tar.gz .

RUN cd or-tools_*_v* && make test
