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
ARG TARGETARCH=amd64
RUN wget \
https://github.com/bazelbuild/bazelisk/releases/download/v1.27.0/bazelisk-linux-${TARGETARCH} \
&& chmod +x bazelisk-linux-${TARGETARCH} \
&& mv bazelisk-linux-${TARGETARCH} /usr/local/bin/bazel

# Install Java JDK and Maven
RUN zypper refresh \
&& zypper install -y java-17-openjdk-devel maven \
&& zypper clean -a
ENV PATH=/usr/share/maven/bin:$PATH

# Install Python
RUN zypper update -y \
&& zypper install -y python3 python3-pip python3-devel \
&& zypper clean -a

FROM env AS devel
WORKDIR /home/project
COPY . .

FROM devel AS build
RUN bazel version
RUN bazel build --config=ci //ortools/... //examples/...

FROM build AS test
RUN bazel test --config=ci //ortools/... //examples/...
