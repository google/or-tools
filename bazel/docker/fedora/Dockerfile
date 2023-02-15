# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/fedora
FROM fedora:latest AS env
# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN dnf -y update \
&& dnf -y install git \
&& dnf -y groupinstall "Development Tools" \
&& dnf -y install gcc-c++ zlib-devel \
&& dnf -y install dnf-plugins-core \
&& dnf -y copr enable vbatts/bazel \
&& dnf -y install bazel5 \
&& dnf clean all

# Install Python
RUN dnf -y update \
&& dnf -y install python3 python3-devel python3-pip \
&& dnf clean all

FROM env AS devel
WORKDIR /home/project
COPY . .

FROM devel AS build
RUN bazel version
RUN bazel build -c opt --cxxopt=-std=c++17 ...

FROM build AS test
RUN bazel test -c opt --cxxopt=-std=c++17 --test_output=errors ...
