FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update -qq \
&& apt-get install -yq build-essential zlib1g-dev \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

WORKDIR /root
ADD or-tools_amd64_ubuntu-22.04_v*.tar.gz .

RUN cd or-tools_*_v* && make test_cc
