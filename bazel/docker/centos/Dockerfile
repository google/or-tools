# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/centos
FROM centos:latest AS env
LABEL maintainer="corentinl@google.com"
# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN dnf -y update \
&& dnf -y install git wget zlib-devel \
&& dnf -y groupinstall "Development Tools" \
&& dnf clean all \
&& rm -rf /var/cache/dnf
# Install bazel
# see: https://docs.bazel.build/versions/master/install-redhat.html#installing-on-centos-7
RUN dnf config-manager --add-repo \
https://copr.fedorainfracloud.org/coprs/vbatts/bazel/repo/epel-8/vbatts-bazel-epel-8.repo \
&& dnf -y install bazel4 \
&& dnf clean all \
&& rm -rf /var/cache/dnf

FROM env AS devel
WORKDIR /home/lib
COPY . .

FROM devel as build
RUN bazel version
RUN bazel build --curses=no --cxxopt=-std=c++17 --copt='-Wno-sign-compare' //...:all

FROM build as test
RUN bazel test -c opt --curses=no --cxxopt=-std=c++17 --copt='-Wno-sign-compare' //...:all
