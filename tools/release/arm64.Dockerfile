# To build it on x86_64 please read
# https://github.com/multiarch/qemu-user-static#getting-started

# Use yum
#FROM --platform=linux/arm64 centos:7 AS env
#FROM quay.io/pypa/manylinux2014_aarch64:latest AS env
# Use dnf
FROM quay.io/pypa/manylinux_2_28_aarch64:latest AS env
# note: CMake 3.31.2 and SWIG 4.3.0 are already installed

#############
##  SETUP  ##
#############
RUN dnf -y update \
&& dnf -y group install 'Development Tools' \
&& dnf -y install wget curl \
 pcre2-devel openssl \
 which redhat-lsb-core \
 pkgconfig autoconf libtool zlib-devel \
&& dnf clean all \
&& rm -rf /var/cache/dnf

ENTRYPOINT ["/usr/bin/bash", "-c"]
CMD ["/usr/bin/bash"]

# Install .Net
# see: https://learn.microsoft.com/en-us/dotnet/core/install/linux-scripted-manual#scripted-install
RUN wget -q "https://dot.net/v1/dotnet-install.sh" \
&& chmod a+x dotnet-install.sh \
&& ./dotnet-install.sh -c 3.1 -i /usr/local/bin \
&& ./dotnet-install.sh -c 8.0 -i /usr/local/bin
# Trigger first run experience by running arbitrary cmd
#RUN objdump -p /lib64/libstdc++.so.6
#RUN g++ --version
RUN dotnet --info

# Install Java 8 SDK
RUN dnf -y update \
&& dnf -y install java-1.8.0-openjdk java-1.8.0-openjdk-devel maven \
&& dnf clean all \
&& rm -rf /var/cache/dnf
ENV JAVA_HOME=/usr/lib/jvm/java

# Update maven
ADD https://dlcdn.apache.org/maven/maven-3/3.9.9/binaries/apache-maven-3.9.9-bin.tar.gz /usr/local
RUN mkdir -p /usr/local/maven \
 && tar xzvf /usr/local/apache-maven-3.9.9-bin.tar.gz --strip-components=1 -C /usr/local/maven \
 && rm /usr/local/apache-maven-3.9.9-bin.tar.gz
ENV PATH=/usr/local/maven/bin:$PATH

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
ENV ORTOOLS_GIT_BRANCH=${ORTOOLS_GIT_BRANCH:-main}
ARG ORTOOLS_GIT_SHA1
ENV ORTOOLS_GIT_SHA1=${ORTOOLS_GIT_SHA1:-unknown}
RUN git clone -b "${ORTOOLS_GIT_BRANCH}" --single-branch https://github.com/google/or-tools \
&& cd or-tools \
&& git reset --hard "${ORTOOLS_GIT_SHA1}"

# Build delivery
FROM devel AS delivery
WORKDIR /root/or-tools

ARG ORTOOLS_TOKEN
ENV ORTOOLS_TOKEN=${ORTOOLS_TOKEN}
ARG ORTOOLS_DELIVERY
ENV ORTOOLS_DELIVERY=${ORTOOLS_DELIVERY:-all}
RUN ./tools/release/build_delivery_linux.sh "${ORTOOLS_DELIVERY}"

# Publish delivery
FROM delivery AS publish
RUN ./tools/release/publish_delivery_linux.sh "${ORTOOLS_DELIVERY}"
