# ref: https://hub.docker.com/_/centos
FROM centos:7 AS env

#############
##  SETUP  ##
#############
RUN yum -y update \
&& yum -y groupinstall 'Development Tools' \
&& yum -y install \
 wget curl \
 pcre2-devel openssl redhat-lsb-core \
 pkgconfig autoconf libtool zlib-devel which \
&& yum clean all \
&& rm -rf /var/cache/yum

# Bump to gcc-11
RUN yum -y update \
&& yum -y install centos-release-scl \
&& yum -y install devtoolset-11 \
&& yum clean all \
&& echo "source /opt/rh/devtoolset-11/enable" >> /etc/bashrc
SHELL ["/usr/bin/bash", "--login", "-c"]
ENTRYPOINT ["/usr/bin/bash", "--login", "-c"]
CMD ["/usr/bin/bash", "--login"]
# RUN g++ --version

# Install CMake 3.28.3
RUN ARCH=$(uname -m) \
&& wget -q "https://cmake.org/files/v3.28/cmake-3.28.3-linux-${ARCH}.sh" \
&& chmod a+x cmake-3.28.3-linux-${ARCH}.sh \
&& ./cmake-3.28.3-linux-${ARCH}.sh --prefix=/usr/local/ --skip-license \
&& rm cmake-3.28.3-linux-${ARCH}.sh

# Install Swig 4.1.1
RUN curl --location-trusted \
 --remote-name "https://downloads.sourceforge.net/project/swig/swig/swig-4.1.1/swig-4.1.1.tar.gz" \
 -o swig-4.1.1.tar.gz \
&& tar xvf swig-4.1.1.tar.gz \
&& rm swig-4.1.1.tar.gz \
&& cd swig-4.1.1 \
&& ./configure --prefix=/usr \
&& make -j 4 \
&& make install \
&& cd .. \
&& rm -rf swig-4.1.1

# Install .Net
# see https://docs.microsoft.com/en-us/dotnet/core/install/linux-centos#centos-7-
RUN rpm -Uvh https://packages.microsoft.com/config/centos/7/packages-microsoft-prod.rpm \
&& yum -y update \
&& yum -y install dotnet-sdk-3.1 dotnet-sdk-6.0 \
&& yum clean all \
&& rm -rf /var/cache/yum
# Trigger first run experience by running arbitrary cmd
RUN dotnet --info

# Install Java 8 SDK
RUN yum -y update \
&& yum -y install java-1.8.0-openjdk java-1.8.0-openjdk-devel maven \
&& yum clean all \
&& rm -rf /var/cache/yum
ENV JAVA_HOME=/usr/lib/jvm/java

# Install Python
RUN yum -y update \
&& yum -y install \
 rh-python38-python rh-python38-python-devel \
 rh-python38-python-pip rh-python38-python-numpy \
&& yum clean all \
&& rm -rf /var/cache/yum \
&& echo "source /opt/rh/rh-python38/enable" >> /etc/bashrc
RUN python -m pip install absl-py mypy mypy-protobuf

ENV TZ=America/Los_Angeles
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

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
