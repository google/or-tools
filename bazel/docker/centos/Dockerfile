# Create a virtual environment with all tools installed
# ref: https://quay.io/repository/centos/centos
FROM quay.io/centos/centos:stream AS env

# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN dnf -y update \
&& dnf -y install git wget gcc-toolset-11 \
&& dnf clean all \
&& rm -rf /var/cache/dnf

RUN echo "source /opt/rh/gcc-toolset-11/enable" >> /etc/bashrc
SHELL ["/bin/bash", "--login", "-c"]

# Install Bazelisk
RUN wget \
https://github.com/bazelbuild/bazelisk/releases/download/v1.17.0/bazelisk-linux-amd64 \
&& chmod +x bazelisk-linux-amd64 \
&& mv bazelisk-linux-amd64 /usr/local/bin/bazel

# Install Python
RUN dnf -y update \
&& dnf -y install python3.11-devel \
&& dnf clean all \
&& rm -rf /var/cache/dnf

FROM env AS devel
WORKDIR /home/project
COPY . .

FROM devel AS build
RUN bazel version
RUN bazel build \
 -c opt \
 --action_env=BAZEL_CXXOPTS="-std=c++17" \
 --subcommands=true \
 ortools/... examples/...

FROM build AS test
RUN bazel test \
 -c opt \
 --action_env=BAZEL_CXXOPTS="-std=c++17" \
 --test_output=errors \
 ortools/... examples/...
