# ref: https://hub.docker.com/_/ubuntu
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update -qq \
&& apt-get install -yq build-essential zlib1g-dev \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Java/Maven Install
RUN apt-get update -qq \
&& DEBIAN_FRONTEND=noninteractive apt-get install -yq openjdk-21-jdk maven \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
ENV JAVA_HOME=/usr/lib/jvm/java-openjdk

WORKDIR /root
ADD or-tools_amd64_ubuntu-24.04_java_v*.tar.gz .

RUN cd or-tools_*_v* && make test
