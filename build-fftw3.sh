#!/bin/bash
FFTW_VERSION=3.3.8
git clone -b fftw-${FFTW_VERSION} --single-branch https://github.com/FFTW/fftw3.git dependencies/sources/fftw-${FFTW_VERSION}
sudo apt-get update 
sudo apt-get install ocaml
sudo apt-get install ocamlbuild
sudo apt-get install indent
#sudo apt-get install fig2dev
sudo apt-get install fig2dev --fix-missing
sudo apt-get install makeinfo
sudo apt-get install texinfo

# install fftw3
cd dependencies/sources/fftw-${FFTW_VERSION}
./bootstrap.sh
./configure --prefix=dependencies/install
make install
