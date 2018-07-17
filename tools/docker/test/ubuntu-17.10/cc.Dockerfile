FROM ubuntu:17.10

RUN apt update \
&& apt install -y -q build-essential zlib1g-dev \
&& apt clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

#ENV TZ=America/Los_Angeles
#RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

WORKDIR /root
ADD or-tools_ubuntu-17.10_v*.tar.gz .
