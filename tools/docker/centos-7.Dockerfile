FROM centos/devtoolset-7-toolchain-centos7
USER root

#FROM centos:7
#RUN yum -y update \
#&& yum install -y centos-release-scl \
#&& yum install -y devtoolset-7-gcc* \
#&& scl enable devtoolset-7 bash

#############
##  SETUP  ##
#############
RUN yum -y update \
&& yum -y install yum-utils \
&& yum -y install \
 wget git pkg-config make autoconf libtool zlib-devel gawk gcc-c++ curl subversion \
 redhat-lsb-core pcre-devel which \
&& yum clean all \
&& rm -rf /var/cache/yum

# Install CMake 3
RUN yum -y install epel-release \
&& yum install -y cmake3 \
&& ln -s /usr/bin/cmake3 /usr/local/bin/cmake

# Install Swig
RUN wget "https://downloads.sourceforge.net/project/swig/swig/swig-4.0.1/swig-4.0.1.tar.gz" \
&& tar xvf swig-4.0.1.tar.gz \
&& rm swig-4.0.1.tar.gz \
&& cd swig-4.0.1 \
&& ./configure --prefix=/usr \
&& make -j 4 \
&& make install \
&& cd .. \
&& rm -rf swig-4.0.1

# Install Java 8 SDK
RUN yum -y update \
&& yum -y install java-1.8.0-openjdk  java-1.8.0-openjdk-devel \
&& yum clean all \
&& rm -rf /var/cache/yum

# Install dotnet
RUN rpm -Uvh "https://packages.microsoft.com/config/rhel/7/packages-microsoft-prod.rpm" \
&& yum -y update \
&& yum -y install dotnet-sdk-2.2 \
&& yum clean all \
&& rm -rf /var/cache/yum

ENV TZ=America/Los_Angeles
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

# Copy the snk key
COPY or-tools.snk /root/or-tools.snk
ENV DOTNET_SNK=/root/or-tools.snk

################
##  OR-TOOLS  ##
################
ARG SRC_GIT_BRANCH
ENV SRC_GIT_BRANCH ${SRC_GIT_BRANCH:-master}
ARG SRC_GIT_SHA1
ENV SRC_GIT_SHA1 ${SRC_GIT_SHA1:-unknown}

# Download sources
# use SRC_GIT_SHA1 to modify the command
# i.e. avoid docker reusing the cache when new commit is pushed
WORKDIR /root
RUN git clone -b "${SRC_GIT_BRANCH}" --single-branch https://github.com/google/or-tools \
&& echo "sha1: $(cd or-tools && git rev-parse --verify HEAD)" \
&& echo "expected sha1: ${SRC_GIT_SHA1}"

# Prebuild
WORKDIR /root/or-tools
RUN make detect && make third_party
RUN make detect_cc && make cc
RUN make detect_java && make java
RUN make detect_dotnet && make dotnet
