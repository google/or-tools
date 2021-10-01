# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/alpine
FROM alpine:edge AS env
LABEL maintainer="corentinl@google.com"
# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN apk add --no-cache git build-base linux-headers cmake xfce4-dev-tools
ENTRYPOINT ["/bin/sh", "-c"]
CMD ["/bin/sh"]

# SWIG
RUN apk add --no-cache swig

# Python
RUN apk add --no-cache python3-dev py3-pip py3-wheel
RUN python3 -m pip install absl-py mypy-protobuf

################
##  OR-TOOLS  ##
################
FROM env AS devel
ENV SRC_GIT_URL https://github.com/google/or-tools

ARG SRC_GIT_BRANCH
ENV SRC_GIT_BRANCH ${SRC_GIT_BRANCH:-master}
ARG SRC_GIT_SHA1
ENV SRC_GIT_SHA1 ${SRC_GIT_SHA1:-unknown}

# Download sources
# use SRC_GIT_SHA1 to modify the command
# i.e. avoid docker reusing the cache when new commit is pushed
WORKDIR /root
RUN git clone -b "${SRC_GIT_BRANCH}" --single-branch "$SRC_GIT_URL" \
&& echo "sha1: $(cd or-tools && git rev-parse --verify HEAD)" \
&& echo "expected sha1: ${SRC_GIT_SHA1}"

# Build project
FROM devel AS build
WORKDIR /root/or-tools
RUN cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=Release -DBUILD_DEPS=ON -DBUILD_PYTHON=ON
RUN cmake --build build -v -j8
# Rename wheel package ortools-version+musl-....
RUN cp build/python/dist/ortools-*.whl .
RUN NAME=$(ls *.whl | sed -e "s/\(ortools-[0-9\.]\+\)/\1+musl/") && mv *.whl "${NAME}"
RUN rm build/python/dist/ortools-*.whl
RUN mv *.whl build/python/dist/

FROM build AS test
RUN cmake --build build --target test
