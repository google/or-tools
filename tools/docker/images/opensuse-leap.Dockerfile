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

# Install CMake 3.23.2
RUN ARCH=$(uname -m) \
&& wget -q "https://cmake.org/files/v3.23/cmake-3.23.2-linux-${ARCH}.sh" \
&& chmod a+x cmake-3.23.2-linux-${ARCH}.sh \
&& ./cmake-3.23.2-linux-${ARCH}.sh --prefix=/usr/local/ --skip-license \
&& rm cmake-3.23.2-linux-${ARCH}.sh

# Install SWIG
RUN zypper refresh \
&& zypper install -y swig \
&& zypper clean -a

# Install .Net
RUN zypper refresh \
&& zypper install -y wget tar gzip libicu-devel
RUN mkdir -p /usr/share/dotnet \
&& ln -s /usr/share/dotnet/dotnet /usr/bin/dotnet
## .Net 3.1
## see: https://dotnet.microsoft.com/download/dotnet-core/3.1
RUN dotnet_sdk_version=3.1.415 \
&& wget -qO dotnet.tar.gz \
"https://dotnetcli.azureedge.net/dotnet/Sdk/${dotnet_sdk_version}/dotnet-sdk-${dotnet_sdk_version}-linux-x64.tar.gz" \
&& dotnet_sha512='df7a6d1abed609c382799a8f69f129ec72ce68236b2faecf01aed4c957a40a9cfbbc9126381bf517dff3dbe0e488f1092188582701dd0fef09a68b8c5707c747' \
&& echo "$dotnet_sha512  dotnet.tar.gz" | sha512sum -c - \
&& tar -C /usr/share/dotnet -oxzf dotnet.tar.gz \
&& rm dotnet.tar.gz
## .Net 6.0
## see: https://dotnet.microsoft.com/download/dotnet-core/6.0
RUN dotnet_sdk_version=6.0.100 \
&& wget -qO dotnet.tar.gz \
"https://dotnetcli.azureedge.net/dotnet/Sdk/${dotnet_sdk_version}/dotnet-sdk-${dotnet_sdk_version}-linux-x64.tar.gz" \
&& dotnet_sha512='cb0d174a79d6294c302261b645dba6a479da8f7cf6c1fe15ae6998bc09c5e0baec810822f9e0104e84b0efd51fdc0333306cb2a0a6fcdbaf515a8ad8cf1af25b' \
&& echo "$dotnet_sha512  dotnet.tar.gz" | sha512sum -c - \
&& tar -C /usr/share/dotnet -oxzf dotnet.tar.gz \
&& rm dotnet.tar.gz
# Trigger first run experience by running arbitrary cmd
RUN dotnet --info

# Install Java (openjdk-8)
RUN zypper install -y java-1_8_0-openjdk java-1_8_0-openjdk-devel maven \
&& zypper clean -a

# Install Python
RUN zypper install -y python3-devel python3-pip python3-wheel \
&& zypper clean -a
#RUN python3 -m pip install absl-py mypy-protobuf

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
