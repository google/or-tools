FROM quay.io/pypa/manylinux2014_x86_64:latest AS env
# note: CMake 3.30.5 and SWIG 4.2.1 are already installed

RUN yum -y update \
&& yum -y install \
 curl wget \
 git patch \
 which pkgconfig autoconf libtool \
 make gcc-c++ \
 redhat-lsb openssl-devel pcre2-devel \
 zlib-devel unzip zip \
&& yum clean all \
&& rm -rf /var/cache/yum
ENTRYPOINT ["/usr/bin/bash", "-c"]
CMD ["/usr/bin/bash"]

ENV TZ=America/Los_Angeles
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

################
##  OR-TOOLS  ##
################
FROM env AS devel
ENV GIT_URL https://github.com/google/or-tools

ARG GIT_BRANCH
ENV GIT_BRANCH ${GIT_BRANCH:-main}
ARG GIT_SHA1
ENV GIT_SHA1 ${GIT_SHA1:-unknown}

# Download sources
# use GIT_SHA1 to modify the command
# i.e. avoid docker reusing the cache when new commit is pushed
RUN git clone -b "${GIT_BRANCH}" --single-branch "$GIT_URL" /project \
&& cd /project \
&& git reset --hard "${GIT_SHA1}"
WORKDIR /project

COPY build-manylinux.sh .
RUN chmod a+x "build-manylinux.sh"

FROM devel AS build
ENV PLATFORM x86_64
ARG PYTHON_VERSION
ENV PYTHON_VERSION ${PYTHON_VERSION:-3}
RUN ./build-manylinux.sh build

FROM build as test
RUN ./build-manylinux.sh test
