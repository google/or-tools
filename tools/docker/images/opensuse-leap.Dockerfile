# Create a virtual environment with all tools installed
# ref: https://hub.docker.com/r/opensuse/leap
FROM opensuse/leap AS env

#############
##  SETUP  ##
#############
ENV PATH=/usr/local/bin:$PATH
RUN zypper refresh \
&& zypper install -y git gcc11 gcc11-c++ cmake \
 wget which lsb-release util-linux pkgconfig autoconf libtool gzip zlib-devel \
&& zypper clean -a
ENV CC=gcc-11 CXX=g++-11
ENTRYPOINT ["/usr/bin/bash", "-c"]
CMD ["/usr/bin/bash"]

# Install SWIG
RUN zypper refresh \
&& zypper install -y swig \
&& zypper clean -a

# Install .Net
RUN zypper refresh \
&& zypper install -y wget tar gzip libicu-devel
# see: https://learn.microsoft.com/en-us/dotnet/core/install/linux-scripted-manual#scripted-install
RUN wget -q "https://dot.net/v1/dotnet-install.sh" \
&& chmod a+x dotnet-install.sh \
&& ./dotnet-install.sh -c 3.1 -i /usr/local/bin \
&& ./dotnet-install.sh -c 8.0 -i /usr/local/bin
# Trigger first run experience by running arbitrary cmd
RUN dotnet --info

# Install Java (openjdk-8)
RUN zypper install -y java-1_8_0-openjdk java-1_8_0-openjdk-devel maven \
&& zypper clean -a

# Install Python
RUN zypper install -y python311-devel python311-pip \
&& zypper clean -a
RUN python3.11 -m pip install absl-py mypy mypy-protobuf

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

ARG OR_TOOLS_PATCH
ENV OR_TOOLS_PATCH ${OR_TOOLS_PATCH:-9999}

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
ENV USE_DOTNET_CORE_31=ON
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
