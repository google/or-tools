# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/rockylinux
FROM rockylinux:9 AS env

# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN dnf -y update \
&& dnf -y install git wget zlib-devel gcc-toolset-13 \
&& dnf -y group install "Development Tools" \
&& dnf clean all \
&& rm -rf /var/cache/dnf

RUN echo "source /opt/rh/gcc-toolset-13/enable" >> /etc/bashrc
SHELL ["/bin/bash", "--login", "-c"]

# Install Bazelisk
RUN wget \
https://github.com/bazelbuild/bazelisk/releases/download/v1.25.0/bazelisk-linux-amd64 \
&& chmod +x bazelisk-linux-amd64 \
&& mv bazelisk-linux-amd64 /usr/local/bin/bazel

# Install Java
RUN dnf -y update \
&& dnf -y install java-11-openjdk java-11-openjdk-devel maven \
&& dnf clean all
ENV JAVA_HOME=/usr/lib/jvm/java-openjdk

# Install Python
RUN dnf -y update \
&& dnf -y install python3-devel python3-pip python3-numpy \
&& dnf clean all \
&& rm -rf /var/cache/dnf

# Add the library src to our build env
FROM env AS devel
WORKDIR /home/project
COPY . .

FROM devel AS build
RUN bazel version
RUN bazel build --config=ci //ortools/... //examples/...

FROM build AS test
RUN bazel test --config=ci //ortools/... //examples/...
