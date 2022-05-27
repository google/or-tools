# ref: https://hub.docker.com/_/ubuntu
FROM ubuntu:18.04

RUN apt-get update \
&& apt-get install -yq make \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

WORKDIR /root
ADD or-tools_amd64_ubuntu-18.04_python_v*.tar.gz .

RUN cd or-tools_*_v* && make test
