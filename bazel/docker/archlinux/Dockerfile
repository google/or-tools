# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/archlinux/
FROM archlinux:latest AS env

# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN pacman -Syu --noconfirm git base-devel bazel

# Install Python
RUN pacman -Syu --noconfirm python python-pip python-setuptools \
 python-numpy python-pandas

FROM env AS devel
WORKDIR /home/project
COPY . .

FROM devel AS build
RUN bazel version
RUN bazel build --config=ci //ortools/... //examples/...

FROM build AS test
RUN bazel test --config=ci //ortools/... //examples/...
