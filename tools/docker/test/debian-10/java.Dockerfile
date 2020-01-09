FROM debian:10
LABEL maintainer="corentinl@google.com"

RUN apt-get update \
&& apt-get install -y -q build-essential zlib1g-dev default-jdk \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

#ENV TZ=America/Los_Angeles
#RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

WORKDIR /root
ADD or-tools_debian-10_v*.tar.gz .
