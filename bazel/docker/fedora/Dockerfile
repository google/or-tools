# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/fedora
FROM fedora:latest AS env
LABEL maintainer="corentinl@google.com"
# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN dnf -y update \
&& dnf -y install git \
&& dnf -y groupinstall "Development Tools" \
&& dnf -y install gcc-c++ zlib-devel \
&& dnf -y install dnf-plugins-core \
&& dnf -y copr enable vbatts/bazel \
&& dnf -y install bazel4 \
&& dnf clean all

FROM env AS devel
WORKDIR /home/project
COPY . .

FROM devel AS build
RUN bazel version
RUN bazel build --curses=no --cxxopt=-std=c++20 --copt='-Wno-sign-compare' //...:all

FROM build AS test
RUN bazel test -c opt --curses=no --cxxopt=-std=c++20 --copt='-Wno-sign-compare' //...:all
