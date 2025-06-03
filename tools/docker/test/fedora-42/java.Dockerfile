# ref: https://hub.docker.com/_/fedora
FROM fedora:42

RUN dnf -y update \
&& dnf -y install git \
 wget which lsb_release pkgconfig autoconf libtool zlib-devel \
&& dnf -y install @development-tools \
&& dnf -y install gcc-c++ cmake \
&& dnf clean all

# Java Install
RUN dnf -y update \
&& dnf -y install java-21-openjdk java-21-openjdk-devel maven \
&& dnf clean all
ENV JAVA_HOME=/usr/lib/jvm/java-openjdk

WORKDIR /root
ADD or-tools_amd64_fedora-42_java_v*.tar.gz .

RUN cd or-tools_*_v* && make test
