FROM ubuntu:16.04

RUN apt update \
&& apt install -y -q build-essential zlib1g-dev default-jdk maven \
&& apt clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
ENV JAVA_HOME=/usr/lib/jvm/default-java

# Install gcc 7
RUN apt update -qq \
&& apt install -yq software-properties-common \
&& add-apt-repository -y ppa:ubuntu-toolchain-r/test \
&& apt update -qq \
&& apt install -yq g++-7 \
&& apt clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
# Configure alias
RUN update-alternatives \
 --install /usr/bin/gcc gcc /usr/bin/gcc-7 60 \
 --slave /usr/bin/g++ g++ /usr/bin/g++-7 \
 --slave /usr/bin/gcov gcov /usr/bin/gcov-7 \
 --slave /usr/bin/gcov-tool gcov-tool /usr/bin/gcov-tool-7 \
 --slave /usr/bin/gcc-ar gcc-ar /usr/bin/gcc-ar-7 \
 --slave /usr/bin/gcc-nm gcc-nm /usr/bin/gcc-nm-7 \
 --slave /usr/bin/gcc-ranlib gcc-ranlib /usr/bin/gcc-ranlib-7

#ENV TZ=America/Los_Angeles
#RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

WORKDIR /root
ADD or-tools_ubuntu-16.04_v*.tar.gz .

RUN cd or-tools_*_v* && make test_java
