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

# Install Swig
RUN pacman -Syu --noconfirm swig

# Install .Net
RUN pacman -Syu --noconfirm dotnet-sdk
# Trigger first run experience by running arbitrary cmd
RUN dotnet --info

# Install Java
RUN pacman -Syu --noconfirm jdk-openjdk maven
ENV JAVA_HOME=/usr/lib/jvm/default

# Install Python
RUN pacman -Syu --noconfirm python python-pip \
 python-wheel python-virtualenv python-setuptools \
 python-numpy python-pandas
RUN python -m pip install --break-system-package \
 absl-py mypy mypy-protobuf

################
##  OR-TOOLS  ##
################
FROM env AS devel
ENV DISTRIBUTION=archlinux

WORKDIR /root
# Copy the snk key
COPY or-tools.snk /root/or-tools.snk
ENV DOTNET_SNK=/root/or-tools.snk

ARG SRC_GIT_BRANCH
ENV SRC_GIT_BRANCH=${SRC_GIT_BRANCH:-main}
ARG SRC_GIT_SHA1
ENV SRC_GIT_SHA1 ${SRC_GIT_SHA1:-unknown}

ARG OR_TOOLS_PATCH
ENV OR_TOOLS_PATCH=${OR_TOOLS_PATCH:-9999}

# Download sources
# use SRC_GIT_SHA1 to modify the command
# i.e. avoid docker reusing the cache when new commit is pushed
RUN git clone -b "${SRC_GIT_BRANCH}" --single-branch --depth=1 https://github.com/google/or-tools \
&& [[ $(cd or-tools && git rev-parse --verify HEAD) == ${SRC_GIT_SHA1} ]]
WORKDIR /root/or-tools

# C++
## build
FROM devel AS cpp_build
RUN make detect_cpp \
&& make cpp JOBS=8
## archive
FROM cpp_build AS cpp_archive
RUN make archive_cpp

# .Net
## build
FROM cpp_build AS dotnet_build
RUN make detect_dotnet \
&& make dotnet JOBS=8
## archive
FROM dotnet_build AS dotnet_archive
RUN make archive_dotnet

# Java
## build
FROM cpp_build AS java_build
RUN make detect_java \
&& make java JOBS=8
## archive
FROM java_build AS java_archive
RUN make archive_java

# Python
## build
FROM cpp_build AS python_build
RUN make detect_python \
&& make python JOBS=8
## archive
FROM python_build AS python_archive
RUN make archive_python
