# ref: https://hub.docker.com/_/ubuntu
FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update -qq \
&& apt-get install -yq build-essential zlib1g-dev openjdk-21-jdk maven \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
ENV JAVA_HOME=/usr/lib/jvm/java-openjdk

#ENV TZ=America/Los_Angeles
#RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

WORKDIR /root
ADD or-tools_amd64_ubuntu-20.04_java_v*.tar.gz .

RUN cd or-tools_*_v* && make test
