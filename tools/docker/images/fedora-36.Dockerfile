# ref: https://hub.docker.com/_/fedora
FROM fedora:36 AS env

#############
##  SETUP  ##
#############
RUN dnf -y update \
&& dnf -y install git \
 wget which redhat-lsb-core pkgconfig autoconf libtool zlib-devel \
&& dnf -y groupinstall "Development Tools" \
&& dnf -y install gcc-c++ cmake \
&& dnf clean all
ENTRYPOINT ["/usr/bin/bash", "-c"]
CMD ["/usr/bin/bash"]

RUN dnf -y update \
&& dnf -y install swig \
&& dnf clean all

# Java Install
RUN dnf -y update \
&& dnf -y install java-11-openjdk java-11-openjdk-devel maven \
&& dnf clean all
ENV JAVA_HOME=/usr/lib/jvm/java-openjdk

# .Net Install
# see: https://docs.microsoft.com/en-us/dotnet/core/install/linux-fedora
RUN dnf -y update \
&& dnf -y install dotnet-sdk-3.1 dotnet-sdk-6.0 \
&& dnf clean all
# Trigger first run experience by running arbitrary cmd
RUN dotnet --info

################
##  OR-TOOLS  ##
################
FROM env AS devel
WORKDIR /root
# Copy the snk key
COPY or-tools.snk /root/or-tools.snk
ENV DOTNET_SNK=/root/or-tools.snk

ARG SRC_GIT_BRANCH
ENV SRC_GIT_BRANCH ${SRC_GIT_BRANCH:-main}
ARG SRC_GIT_SHA1
ENV SRC_GIT_SHA1 ${SRC_GIT_SHA1:-unknown}

# Download sources
# use SRC_GIT_SHA1 to modify the command
# i.e. avoid docker reusing the cache when new commit is pushed
RUN git clone -b "${SRC_GIT_BRANCH}" --single-branch https://github.com/google/or-tools \
&& echo "sha1: $(cd or-tools && git rev-parse --verify HEAD)" \
&& echo "expected sha1: ${SRC_GIT_SHA1}"

# Build third parties
FROM devel AS build
WORKDIR /root/or-tools

ARG BUILD_LANG
ENV BUILD_LANG ${BUILD_LANG:-cpp}

RUN make detect_${BUILD_LANG} \
&& make ${BUILD_LANG} JOBS=4

# Create archive
FROM build AS archive
RUN make archive_${BUILD_LANG}
