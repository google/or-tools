# To build it on x86_64 please read
# https://github.com/multiarch/qemu-user-static#getting-started
FROM quay.io/pypa/manylinux_2_28_aarch64:latest AS env
# note: Almalinux:8 based image with
# CMake 3.31.2 and SWIG 4.3.0 already installed

RUN dnf -y update \
&& dnf -y install \
 curl wget \
 git patch \
 which pkgconfig autoconf libtool \
 make gcc-c++ \
 redhat-lsb openssl-devel pcre2-devel \
 zlib-devel unzip zip \
&& dnf clean all \
&& rm -rf /var/cache/dnf
ENTRYPOINT ["/usr/bin/bash", "-c"]
CMD ["/usr/bin/bash"]

ENV TZ=America/Los_Angeles
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

################
##  OR-TOOLS  ##
################
FROM env AS devel
ENV GIT_URL=https://github.com/google/or-tools

ARG GIT_BRANCH
ENV GIT_BRANCH=${GIT_BRANCH:-main}
ARG GIT_SHA1
ENV GIT_SHA1=${GIT_SHA1:-unknown}

# Download sources
# use GIT_SHA1 to modify the command
# i.e. avoid docker reusing the cache when new commit is pushed
RUN git clone -b "${GIT_BRANCH}" --single-branch "$GIT_URL" /project \
&& cd /project \
&& git reset --hard "${GIT_SHA1}"
WORKDIR /project

# Copy build script and setup env
ENV PLATFORM=aarch64
ARG PYTHON_VERSION
ENV PYTHON_VERSION=${PYTHON_VERSION:-3}
COPY build-manylinux.sh .
RUN chmod a+x "build-manylinux.sh"

FROM devel AS build
RUN ./build-manylinux.sh build

FROM build AS test
RUN ./build-manylinux.sh test
