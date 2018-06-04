FROM ubuntu:14.04

#############
##  SETUP  ##
#############
RUN apt-get update \
&& apt-get install -qq \
 git pkg-config wget make cmake3 autoconf libtool zlib1g-dev gawk g++ curl subversion lsb-release libpcre3-dev \
 python-dev python-wheel python-setuptools python-six \
 python3-dev python3-wheel python3-setuptools \
 default-jdk \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Dotnet Install
# note: package "apt-transport-https" is needed by deb command see below
RUN apt-get update \
&& wget -q https://packages.microsoft.com/config/ubuntu/14.04/packages-microsoft-prod.deb \
&& dpkg -i packages-microsoft-prod.deb \
&& apt-get install -qq apt-transport-https \
&& apt-get update \
&& apt-get install -qq dotnet-sdk-2.1 \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

## Mono Install
#RUN apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv-keys 3FA7E0328081BFF6A14DA29AA6A19B38D3D831EF \
#&& echo "deb https://download.mono-project.com/repo/ubuntu stable-trusty main" | sudo tee /etc/apt/sources.list.d/mono-official-stable.list \
#&& apt-get update \
#&& apt-get install -qq mono-complete \
#&& apt-get clean \
#&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Swig install
RUN wget "https://downloads.sourceforge.net/project/swig/swig/swig-3.0.12/swig-3.0.12.tar.gz" \
&& tar xvf swig-3.0.12.tar.gz && rm swig-3.0.12.tar.gz \
&& cd swig-3.0.12 && ./configure --prefix=/usr && make -j 4 && make install \
&& cd .. && rm -rf swig-3.0.12

ENV TZ=America/Los_Angeles
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

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
RUN make detect_python && make python
RUN make detect_java && make java
RUN make detect_dotnet && make dotnet
