# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/_/archlinux/
FROM archlinux:latest AS env

#############
##  SETUP  ##
#############
ENV PATH=/usr/local/bin:$PATH
RUN pacman -Syu --noconfirm git base-devel cmake
ENTRYPOINT ["/bin/bash", "-c"]
CMD [ "/bin/bash" ]

# Install SWIG
RUN pacman -Syu --noconfirm swig

# Install Python
RUN pacman -Syu --noconfirm python python-pip
RUN python -m pip install absl-py mypy-protobuf

# Install Java
RUN pacman -Syu --noconfirm jdk-openjdk maven
ENV JAVA_HOME=/usr/lib/jvm/default

# Install .NET
RUN pacman -Syu --noconfirm dotnet-sdk
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
