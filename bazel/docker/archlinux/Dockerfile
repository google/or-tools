# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/archlinux/
FROM archlinux:latest AS env
LABEL maintainer="corentinl@google.com"
# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN pacman -Syu --noconfirm git base-devel bazel

FROM env AS devel
WORKDIR /home/project
COPY . .

FROM devel AS build
RUN bazel version
RUN bazel build --curses=no --cxxopt=-std=c++20 --copt='-Wno-sign-compare' //...:all

FROM build AS test
RUN bazel test -c opt --curses=no --cxxopt=-std=c++20 --copt='-Wno-sign-compare' //...:all
