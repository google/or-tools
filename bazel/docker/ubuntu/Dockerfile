# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/ubuntu
FROM ubuntu:rolling AS env

# Install system build dependencies
ENV TZ=Europe/Paris
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

# Install system build dependencies
RUN apt update -qq \
&& apt install -yq git wget curl libssl-dev build-essential \
&& apt install -yq python3-dev python3-pip python3-venv \
 python3-numpy python3-pandas \
&& apt install -yq default-jdk \
&& apt clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Install Bazel
RUN apt install -y apt-transport-https curl gnupg \
&& curl -fsSL https://bazel.build/bazel-release.pub.gpg | gpg --dearmor >bazel-archive-keyring.gpg \
&& mv bazel-archive-keyring.gpg /usr/share/keyrings
ARG TARGETARCH=amd64
RUN echo "deb [arch=$TARGETARCH signed-by=/usr/share/keyrings/bazel-archive-keyring.gpg] https://storage.googleapis.com/bazel-apt stable jdk1.8" | tee /etc/apt/sources.list.d/bazel.list
RUN apt update -qq \
&& apt install -yq bazel \
&& apt clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

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
