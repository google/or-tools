FROM centos:7

#############
##  SETUP  ##
#############
RUN yum -y update \
&& yum -y install yum-utils \
&& yum -y install \
 wget git pkg-config make autoconf libtool zlib-devel gawk gcc-c++ curl subversion \
 redhat-lsb-core pcre-devel which \
 python-devel python-setuptools python-six python-wheel \
 java-1.8.0-openjdk  java-1.8.0-openjdk-devel \
&& yum clean all \
&& rm -rf /var/cache/yum

# Install dotnet
RUN rpm -Uvh "https://packages.microsoft.com/config/rhel/7/packages-microsoft-prod.rpm" \
&& yum -y update \
&& yum -y install dotnet-sdk-2.1 \
&& yum clean all \
&& rm -rf /var/cache/yum

# Install Mono
#RUN rpm --import "https://keyserver.ubuntu.com/pks/lookup?op=get&search=0x3FA7E0328081BFF6A14DA29AA6A19B38D3D831EF" \
#&& su -c 'curl https://download.mono-project.com/repo/centos7-stable.repo | tee /etc/yum.repos.d/mono-centos7-stable.repo'
#&& yum -y update \
#&& yum -y install mono-devel

# Install CMake
RUN wget "https://cmake.org/files/v3.8/cmake-3.8.2-Linux-x86_64.sh" \
&& chmod 775 cmake-3.8.2-Linux-x86_64.sh \
&& yes | ./cmake-3.8.2-Linux-x86_64.sh --prefix=/usr --exclude-subdir

# Install Swig
RUN wget "https://downloads.sourceforge.net/project/swig/swig/swig-3.0.12/swig-3.0.12.tar.gz" \
&& tar xvf swig-3.0.12.tar.gz \
&& rm swig-3.0.12.tar.gz \
&& cd swig-3.0.12 \
&& ./configure --prefix=/usr \
&& make -j 4 \
&& make install \
&& cd .. \
&& rm -rf swig-3.0.12

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
