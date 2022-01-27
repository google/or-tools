FROM ubuntu:21.04

RUN apt-get update -qq \
&& DEBIAN_FRONTEND=noninteractive apt-get install -yq \
 build-essential zlib1g-dev \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

WORKDIR /root
ADD or-tools_ubuntu-21.04_v*.tar.gz .

RUN cd or-tools_*_v* && make test_cc
