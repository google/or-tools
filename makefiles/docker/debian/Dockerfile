# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/debian
FROM debian:latest AS base
# Install system build dependencies
ENV PATH=/usr/local/bin:$PATH
RUN apt-get update -qq \
&& apt-get install -yq \
 git wget libssl-dev build-essential cmake \
 lsb-release \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Install CMake
ARG VERSION=3.25
ARG VERSION_FULL=3.25.2
RUN wget -q "https://cmake.org/files/v${VERSION}/cmake-${VERSION_FULL}-linux-x86_64.sh" \
&& chmod a+x cmake-${VERSION_FULL}-linux-x86_64.sh \
&& ./cmake-${VERSION_FULL}-linux-x86_64.sh --prefix=/usr/local/ --skip-license \
&& rm cmake-${VERSION_FULL}-linux-x86_64.sh
CMD [ "/bin/bash" ]

FROM base AS swig
RUN apt-get update -qq \
&& apt-get install -yq swig \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
