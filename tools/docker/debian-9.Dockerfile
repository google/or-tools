FROM debian:9

ENV SRC_GIT_BRANCH master

RUN apt-get update

RUN apt-get -y install git wget autoconf libtool zlib1g-dev gawk g++ curl cmake subversion make mono-complete swig lsb-release python-dev default-jdk twine python-setuptools python-six python3-setuptools python3-dev python-wheel python3-wheel

ENV TZ=America/Los_Angeles
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

WORKDIR /root

RUN git clone -b "$SRC_GIT_BRANCH" --single-branch https://github.com/google/or-tools

WORKDIR /root/or-tools

RUN make third_party
