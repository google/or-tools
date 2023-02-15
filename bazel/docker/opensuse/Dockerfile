# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/r/opensuse/tumbleweed
FROM opensuse/tumbleweed AS env
# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN zypper refresh \
&& zypper install -y git gcc gcc-c++ zlib-devel \
&& zypper clean -a
ENV CC=gcc CXX=g++

# https://software.opensuse.org/package/bazel4.2
RUN zypper addrepo -Gf \
https://download.opensuse.org/repositories/devel:tools:building/openSUSE_Factory/devel:tools:building.repo \
&& zypper refresh \
&& zypper install -y bazel4.2 findutils \
&& zypper clean -a

# Install Python
RUN zypper install -y python3 python3-pip python3-devel \
&& zypper clean -a

FROM env AS devel
WORKDIR /home/project
COPY . .

FROM devel AS build
RUN bazel version
RUN bazel build -c opt --cxxopt=-std=c++17 ...

FROM build AS test
RUN bazel test -c opt --cxxopt=-std=c++17 --test_output=errors ...
