# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/alpine
FROM alpine:edge AS env
LABEL maintainer="corentinl@google.com"
# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN apk add --no-cache git build-base linux-headers zlib-dev
RUN apk add --no-cache -X http://dl-cdn.alpinelinux.org/alpine/edge/testing bazel

# Install OpenJDK16
# note: default-jvm will now point to java-16-openjdk
RUN apk add --no-cache openjdk16 --repository=http://dl-cdn.alpinelinux.org/alpine/edge/testing/
# Remove infinite loop since jre point to the current directory
# otherwise bazel issue an error and stop...
RUN rm /usr/lib/jvm/default-jvm/jre

ENV JAVA_HOME=/usr/lib/jvm/default-jvm
ENV PATH=$JAVA_HOME/bin:$PATH

FROM env AS devel
WORKDIR /home/lib
COPY . .

FROM devel as build
RUN bazel build --curses=no --host_javabase=@local_jdk//:jdk --cxxopt=-std=c++17 --copt='-Wno-sign-compare' //...:all

FROM build as test
RUN bazel test --curses=no --host_javabase=@local_jdk//:jdk --cxxopt=-std=c++17 --copt='-Wno-sign-compare' //...:all
