FROM ubuntu:17.04

RUN apt-get update

RUN apt-get -y install git autoconf libtool zlib1g-dev gawk g++ curl cmake subversion make mono-complete swig lsb-release python-dev default-jdk twine python-setuptools python-six python3-setuptools python3-dev python-wheel python3-wheel

ENV TZ=America/Los_Angeles
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

WORKDIR /root

RUN git clone https://github.com/google/or-tools

WORKDIR /root/or-tools

RUN make third_party
