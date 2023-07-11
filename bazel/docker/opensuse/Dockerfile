# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/r/opensuse/tumbleweed
FROM opensuse/tumbleweed AS env

# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN zypper update -y \
&& zypper install -y git gcc gcc-c++ zlib-devel \
&& zypper clean -a
ENV CC=gcc CXX=g++

# Install Bazel 6
RUN zypper addrepo -Gf \
https://download.opensuse.org/repositories/home:ftake:branches:M17N/openSUSE_Tumbleweed/home:ftake:branches:M17N.repo \
&& zypper refresh \
&& zypper install -y bazel6 \
&& zypper clean -a

# Install Python
RUN zypper update -y \
&& zypper install -y python3 python3-pip python3-devel \
&& zypper clean -a

FROM env AS devel
WORKDIR /home/project
COPY . .

FROM devel AS build
RUN bazel6 version
RUN bazel6 build \
 -c opt \
 --action_env=BAZEL_CXXOPTS="-std=c++17" \
 --subcommands=true \
 ortools/... examples/...

FROM build AS test
RUN bazel6 test \
 -c opt \
 --action_env=BAZEL_CXXOPTS="-std=c++17" \
 --test_output=errors \
 ortools/... examples/...
