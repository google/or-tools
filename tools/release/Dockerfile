# Need a Centos 7 to avoid this:
# https://github.com/google/or-tools/issues/2520#issuecomment-825119882
# note: PEP 599 manylinux2014 is also based on Centos 7 which will reach End of Life (EOL) on June 30th, 2024
FROM centos:7 AS env

#############
##  SETUP  ##
#############
RUN yum -y update \
&& yum -y groupinstall 'Development Tools' \
&& yum -y install wget curl pcre-devel openssl redhat-lsb-core pkgconfig autoconf libtool zlib-devel which \
&& yum clean all \
&& rm -rf /var/cache/yum

# Bump to gcc-9
RUN yum -y update \
&& yum -y install centos-release-scl \
&& yum -y install devtoolset-9 \
&& yum clean all \
&& echo "source /opt/rh/devtoolset-9/enable" >> /etc/bashrc
SHELL ["/bin/bash", "--login", "-c"]
# RUN gcc --version

# Install CMake 3.21.1
RUN wget -q "https://cmake.org/files/v3.21/cmake-3.21.1-linux-x86_64.sh" \
&& chmod a+x cmake-3.21.1-linux-x86_64.sh \
&& ./cmake-3.21.1-linux-x86_64.sh --prefix=/usr/local/ --skip-license \
&& rm cmake-3.21.1-linux-x86_64.sh

# Install Swig 4.0.2
RUN curl --location-trusted \
 --remote-name "https://downloads.sourceforge.net/project/swig/swig/swig-4.0.2/swig-4.0.2.tar.gz" \
 -o swig-4.0.2.tar.gz \
&& tar xvf swig-4.0.2.tar.gz \
&& rm swig-4.0.2.tar.gz \
&& cd swig-4.0.2 \
&& ./configure --prefix=/usr \
&& make -j 4 \
&& make install \
&& cd .. \
&& rm -rf swig-4.0.2

# Install Java 8 SDK
RUN yum -y update \
&& yum -y install java-1.8.0-openjdk java-1.8.0-openjdk-devel maven \
&& yum clean all \
&& rm -rf /var/cache/yum
ENV JAVA_HOME=/usr/lib/jvm/java

# Install dotnet
# see https://docs.microsoft.com/en-us/dotnet/core/install/linux-centos#centos-7-
RUN rpm -Uvh https://packages.microsoft.com/config/centos/7/packages-microsoft-prod.rpm \
&& yum -y update \
&& yum -y install dotnet-sdk-3.1 dotnet-sdk-5.0 \
&& yum clean all \
&& rm -rf /var/cache/yum
# Trigger first run experience by running arbitrary cmd
RUN dotnet --info

# Openssl 1.1
RUN yum -y update \
&& yum -y install epel-release \
&& yum repolist \
&& yum -y install openssl11

ENV TZ=America/Los_Angeles
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

################
##  OR-TOOLS  ##
################
FROM env AS devel
WORKDIR /root

# Download sources
# use ORTOOLS_GIT_SHA1 to modify the command
# i.e. avoid docker reusing the cache when new commit is pushed
ARG ORTOOLS_GIT_BRANCH
ENV ORTOOLS_GIT_BRANCH ${ORTOOLS_GIT_BRANCH:-master}
ARG ORTOOLS_GIT_SHA1
ENV ORTOOLS_GIT_SHA1 ${ORTOOLS_GIT_SHA1:-unknown}
RUN git clone -b "${ORTOOLS_GIT_BRANCH}" --single-branch https://github.com/google/or-tools \
&& echo "sha1: $(cd or-tools && git rev-parse --verify HEAD)" \
&& echo "expected sha1: ${ORTOOLS_GIT_SHA1}"

# Build delivery
FROM devel AS delivery
WORKDIR /root/or-tools

ENV GPG_ARGS ""

ARG ORTOOLS_TOKEN
ENV ORTOOLS_TOKEN ${ORTOOLS_TOKEN}
ARG ORTOOLS_DELIVERY
ENV ORTOOLS_DELIVERY ${ORTOOLS_DELIVERY:-all}
RUN ./tools/release/build_delivery_linux.sh "${ORTOOLS_DELIVERY}"

# Publish delivery
FROM delivery AS publish
RUN ./tools/release/publish_delivery_linux.sh "${ORTOOLS_DELIVERY}"
