# ref: https://hub.docker.com/_/ubuntu
FROM ubuntu:24.10

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update -qq \
&& apt-get install -yq make \
&& apt-get clean \
&& rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

WORKDIR /root
ADD or-tools_amd64_ubuntu-24.10_python_v*.tar.gz .

RUN cd or-tools_*_v* && make test
