FROM debian:11
LABEL maintainer="corentinl@google.com"

RUN apt-get update \
&& apt-get install -y -q build-essential zlib1g-dev default-jdk maven \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
ENV JAVA_HOME=/usr/lib/jvm/default-java

#ENV TZ=America/Los_Angeles
#RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

WORKDIR /root
ADD or-tools_debian-11_v*.tar.gz .

RUN cd or-tools_*_v* && make test_java
