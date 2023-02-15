# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/archlinux/
FROM archlinux:latest AS env
# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN pacman -Syu --noconfirm git base-devel bazel

# Install Python
RUN pacman -Syu --noconfirm python python-pip
RUN pacman -Syu --noconfirm python python-setuptools

FROM env AS devel
WORKDIR /home/project
COPY . .

FROM devel AS build
RUN bazel version
RUN bazel build -s -c opt --cxxopt=-std=c++17 ...

FROM build AS test
RUN bazel test -c opt --cxxopt=-std=c++17 --test_output=errors ...
