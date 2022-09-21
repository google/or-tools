# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/alpine
FROM alpine:edge AS env
# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN apk add --no-cache git build-base linux-headers cmake xfce4-dev-tools
ENTRYPOINT ["/bin/sh", "-c"]
CMD ["/bin/sh"]

# SWIG
RUN apk add --no-cache swig

# Python
RUN apk add --no-cache python3-dev py3-pip py3-wheel \
 py3-numpy py3-pandas py3-matplotlib
RUN python3 -m pip install absl-py mypy-protobuf

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
WORKDIR /root
RUN git clone -b "${GIT_BRANCH}" --single-branch "$GIT_URL" /project \
&& cd /project \
&& git reset --hard "${GIT_SHA1}"
WORKDIR /project

# Build project
FROM devel AS build
RUN cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=Release -DBUILD_DEPS=ON -DBUILD_PYTHON=ON -DVENV_USE_SYSTEM_SITE_PACKAGES=ON \
 -DBUILD_CXX_SAMPLES=OFF -DBUILD_CXX_EXAMPLES=OFF
RUN cmake --build build -v -j8
# Rename wheel package ortools-version+musl-....
RUN cp build/python/dist/ortools-*.whl .
RUN NAME=$(ls *.whl | sed -e "s/\(ortools-[0-9\.]\+\)/\1+musl/") && mv *.whl "${NAME}"
RUN rm build/python/dist/ortools-*.whl
RUN mv *.whl build/python/dist/

FROM build AS test
RUN cmake --build build --target test
