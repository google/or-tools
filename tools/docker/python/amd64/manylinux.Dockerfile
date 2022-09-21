FROM quay.io/pypa/manylinux2014_x86_64:latest AS env

RUN yum -y update \
&& yum -y install \
 autoconf \
 curl wget \
 gawk \
 gcc-c++ \
 git \
 libtool \
 make \
 openssl-devel \
 patch \
 pcre-devel \
 redhat-lsb \
 subversion \
 which \
 zlib-devel \
 unzip zip \
&& yum clean all \
&& rm -rf /var/cache/yum
ENTRYPOINT ["/usr/bin/bash", "-c"]
CMD ["/usr/bin/bash"]

# Install CMake 3.22.2
RUN wget -q --no-check-certificate "https://cmake.org/files/v3.22/cmake-3.22.2-linux-x86_64.sh" \
&& chmod a+x cmake-3.22.2-linux-x86_64.sh \
&& ./cmake-3.22.2-linux-x86_64.sh --prefix=/usr --skip-license \
&& rm cmake-3.22.2-linux-x86_64.sh

# Install Swig 4.0.2
RUN curl --location-trusted \
 --remote-name "https://downloads.sourceforge.net/project/swig/swig/swig-4.0.2/swig-4.0.2.tar.gz" \
 -o swig-4.0.2.tar.gz \
&& tar xvf swig-4.0.2.tar.gz \
&& rm swig-4.0.2.tar.gz \
&& cd swig-4.0.2 \
&& ./configure --prefix=/usr \
&& make -j 4 \
&& make install \
&& cd .. \
&& rm -rf swig-4.0.2

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
