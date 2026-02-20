# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/fedora
FROM fedora:latest AS env

# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN dnf -y update \
&& dnf -y install git wget \
&& dnf -y install @development-tools \
&& dnf -y install gcc-c++ zlib-devel \
&& dnf -y install dnf-plugins-core \
&& dnf -y install which \
&& dnf clean all

# Install Bazelisk
ARG TARGETARCH=amd64
RUN wget \
https://github.com/bazelbuild/bazelisk/releases/download/v1.27.0/bazelisk-linux-${TARGETARCH} \
&& chmod +x bazelisk-linux-${TARGETARCH} \
&& mv bazelisk-linux-${TARGETARCH} /usr/local/bin/bazel

# Install Java
RUN dnf -y update \
&& dnf -y install java-21-openjdk java-21-openjdk-devel maven \
&& dnf clean all
ENV JAVA_HOME=/usr/lib/jvm/java-openjdk

# Install Python
RUN dnf -y update \
&& dnf -y install python3 python3-devel python3-pip python3-numpy \
&& dnf clean all

FROM env AS devel
WORKDIR /home/project
COPY . .

FROM devel AS build
RUN bazel version
RUN bazel build --java_runtime_version=remotejdk_21 --config=ci //ortools/... //examples/...

FROM build AS test
RUN bazel test --java_runtime_version=remotejdk_21 --config=ci //ortools/... //examples/...
