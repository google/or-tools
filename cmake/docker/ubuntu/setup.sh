#!/usr/bin/env bash

set -x
apt-get update -qq
apt-get install -qq git wget build-essential swig python3-dev python3-pip
python3 -m pip install virtualenv
wget https://cmake.org/files/v3.10/cmake-3.10.2-Linux-x86_64.sh && \
chmod a+x cmake-3.10.2-Linux-x86_64.sh && \
./cmake-3.10.2-Linux-x86_64.sh --prefix=/usr/local/ --skip-license && \
rm cmake-3.10.2-Linux-x86_64.sh

