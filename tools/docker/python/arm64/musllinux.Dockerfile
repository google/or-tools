FROM quay.io/pypa/musllinux_1_2_aarch64:latest AS env
# CMake 3.31.2 and SWIG 4.3.0 already installed

# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN apk add --no-cache git build-base linux-headers xfce4-dev-tools
ENTRYPOINT ["/bin/sh", "-c"]
CMD ["/bin/sh"]

## Python
#RUN apk add --no-cache python3-dev py3-pip py3-wheel py3-virtualenv \
# py3-numpy py3-pandas py3-matplotlib
#RUN rm -f /usr/lib/python3.*/EXTERNALLY-MANAGED \
#&& python3 -m pip install absl-py mypy mypy-protobuf

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
RUN git clone -b "${GIT_BRANCH}" --single-branch "${GIT_URL}" /project \
&& cd /project \
&& git reset --hard "${GIT_SHA1}"
WORKDIR /project

# Copy build script and setup env
ENV PLATFORM=aarch64
ARG PYTHON_VERSION
ENV PYTHON_VERSION=${PYTHON_VERSION:-3}
COPY build-musllinux.sh .
RUN chmod a+x "build-musllinux.sh"

FROM devel AS build
RUN ./build-musllinux.sh build

FROM build as test
RUN ./build-musllinux.sh test
