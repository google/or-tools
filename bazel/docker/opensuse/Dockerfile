# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/r/opensuse/tumbleweed
FROM opensuse/tumbleweed AS env

# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN zypper update -y \
&& zypper install -y git wget gcc gcc-c++ zlib-devel \
&& zypper clean -a
ENV CC=gcc CXX=g++

# Install Bazelisk
RUN wget \
https://github.com/bazelbuild/bazelisk/releases/download/v1.19.0/bazelisk-linux-amd64 \
&& chmod +x bazelisk-linux-amd64 \
&& mv bazelisk-linux-amd64 /usr/local/bin/bazel

# Java
RUN zypper install -y java-11-openjdk-devel \
&& zypper clean -a

ENV JAVA_HOME=/usr/lib64/jvm/java
ENV PATH=$JAVA_HOME/bin:$PATH

# Install Python
RUN zypper update -y \
&& zypper install -y python3 python3-pip python3-devel \
&& zypper clean -a

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
