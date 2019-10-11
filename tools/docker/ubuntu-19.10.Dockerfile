FROM ubuntu:19.10

#############
##  SETUP  ##
#############
RUN apt update -qq \
&& apt install -yq \
 git pkg-config wget make cmake autoconf libtool zlib1g-dev gawk g++ curl subversion \
 lsb-release \
&& apt clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Swig Install
RUN apt-get update -qq \
&& apt-get install -yq swig \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Java install
RUN apt-get update -qq \
&& apt-get install -yq openjdk-8-jdk \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Dotnet Install
RUN apt-get update -qq \
&& apt-get install -yq apt-transport-https \
&& wget -q https://packages.microsoft.com/config/ubuntu/19.04/packages-microsoft-prod.deb \
&& dpkg -i packages-microsoft-prod.deb \
&& apt-get update -qq \
&& apt-get install -yq dotnet-sdk-2.2 \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

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
