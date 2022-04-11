# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/r/opensuse/tumbleweed
FROM opensuse/tumbleweed AS env
LABEL maintainer="corentinl@google.com"
# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN zypper update -y \
&& zypper install -y git gcc gcc-c++ zlib-devel bazel \
&& zypper clean -a
ENV CC=gcc CXX=g++

# Install Python
RUN zypper update -y \
&& zypper install -y python3 python3-pip python3-devel \
&& zypper clean -a

FROM env AS devel
WORKDIR /home/project
COPY . .

FROM devel AS build
RUN bazel version
RUN bazel build -c opt --cxxopt=-std=c++17 ...

FROM build AS test
RUN bazel test -c opt --cxxopt=-std=c++17 ...
