# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/alpine
FROM alpine:edge AS env
# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN apk add --no-cache git build-base linux-headers zlib-dev
RUN apk add --no-cache -X http://dl-cdn.alpinelinux.org/alpine/edge/testing bazel6

ENV JAVA_HOME=/usr/lib/jvm/default-jvm
ENV PATH=$JAVA_HOME/bin:$PATH

# Install Python
RUN apk add --no-cache python3-dev py3-pip \
 py3-setuptools py3-wheel

FROM env AS devel
WORKDIR /home/project
COPY . .

FROM devel AS build
RUN bazel version

# --javabase=@local_jdk_11//:jdk \
# --host_javabase=@local_jdk_11//:jdk \

# --javabase=@bazel_tools//tools/jdk:local_jdk11 \
# --host_javabase=@bazel_tools//tools/jdk:local_jdk11 \
RUN bazel build \
 -c opt \
 --java_language_version=11 \
 --java_runtime_version=local_jdk11 \
 --tool_java_runtime_version=local_jdk11 \
 --subcommands=true \
 //ortools/... //examples/...

FROM build AS test
RUN bazel test \
 --host_javabase=@local_jdk//:jdk \
 -c opt \
 --test_output=errors \
 //ortools/... //examples/...
