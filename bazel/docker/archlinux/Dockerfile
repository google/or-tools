# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/archlinux/
FROM archlinux:latest AS env
# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN pacman -Syu --noconfirm git wget base-devel

# Install Bazelisk
RUN wget \
https://github.com/bazelbuild/bazelisk/releases/download/v1.19.0/bazelisk-linux-amd64 \
&& chmod +x bazelisk-linux-amd64 \
&& mv bazelisk-linux-amd64 /usr/local/bin/bazel

# Java
RUN pacman -Syu --noconfirm jdk11-openjdk
ENV JAVA_HOME=/usr/lib/jvm/default
ENV PATH=${JAVA_HOME}/bin:$PATH

# Install Python
RUN pacman -Syu --noconfirm python python-pip python-setuptools

FROM env AS devel
WORKDIR /home/project
COPY . .

FROM devel AS build
RUN bazel version
RUN bazel build \
 -c opt \
 --subcommands=true \
 //ortools/... //examples/...

FROM build AS test
RUN bazel test \
 -c opt \
 --test_output=errors \
 //ortools/... //examples/...
