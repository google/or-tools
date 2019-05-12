#!/bin/bash
FFTW_VERSION=3.3.8
git clone -b fftw-${FFTW_VERSION} --single-branch https://github.com/FFTW/fftw3.git dependencies/sources/fftw-${FFTW_VERSION}
apt-get update 
apt-get install ocaml
apt-get install ocamlbuild
apt-get install indent
#apt-get install fig2dev
apt-get install fig2dev --fix-missing
apt-get install makeinfo
apt-get install texinfo

# install fftw3
cd dependencies/sources/fftw-${FFTW_VERSION}
./bootstrap.sh
./configure --prefix=dependencies/install
make install
