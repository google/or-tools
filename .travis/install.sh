#!/usr/bin/env bash
set -x
set -e

function installswig() {
  # Need SWIG >= 3.0.8
  cd /tmp/ &&
    wget https://github.com/swig/swig/archive/rel-3.0.12.tar.gz &&
    tar zxf rel-3.0.12.tar.gz && cd swig-rel-3.0.12 &&
    ./autogen.sh && ./configure --prefix "${HOME}"/swig/ 1>/dev/null &&
    make >/dev/null &&
    make install >/dev/null
}

function installdotnetsdk(){
  # Installs for Ubuntu Trusty distro
  sudo apt-get update -qq
  # Need to upgrade to dpkg >= 1.17.5ubuntu5.8,
  # which fixes https://bugs.launchpad.net/ubuntu/+source/dpkg/+bug/1730627
  sudo apt-get install -yq apt-transport-https dpkg
  wget -q https://packages.microsoft.com/config/ubuntu/16.04/packages-microsoft-prod.deb
  sudo dpkg -i packages-microsoft-prod.deb
  # Install dotnet sdk 2.1
  sudo apt-get update -qq
  sudo apt-get install -yqq dotnet-sdk-2.2 dotnet-sdk-3.0
}

################
##  MAKEFILE  ##
################
if [ "${BUILDER}" == make ]; then
  echo 'TRAVIS_OS_NAME = ${TRAVIS_OS_NAME}'
  if [ "${TRAVIS_OS_NAME}" == linux ]; then
    echo 'travis_fold:start:c++'
    sudo apt-get -qq update
    sudo apt-get -yqq install autoconf libtool zlib1g-dev gawk curl	lsb-release
    echo 'travis_fold:end:c++'
    if [ "${LANGUAGE}" != cc ]; then
      echo 'travis_fold:start:swig'
      installswig
      echo 'travis_fold:end:swig'
    fi
    if [ "${LANGUAGE}" == python3 ]; then
      echo 'travis_fold:start:python3'
      pyenv global system 3.7
      python3.7 -m pip install -q virtualenv wheel six
      echo 'travis_fold:end:python3'
    elif [ "${LANGUAGE}" == dotnet ]; then
      echo 'travis_fold:start:dotnet'
      installdotnetsdk
      echo 'travis_fold:end:dotnet'
    fi
  elif [ "${TRAVIS_OS_NAME}" == linux-ppc64le ]; then
    echo 'travis_fold:start:c++'
    sudo apt-get -qq update
    sudo apt-get -yqq install autoconf libtool zlib1g-dev gawk curl	lsb-release
    echo 'travis_fold:end:c++'
    if [ "${LANGUAGE}" != cc ]; then
      echo 'travis_fold:start:swig'
      installswig
      echo 'travis_fold:end:swig'
    fi
    if [ "${LANGUAGE}" == python3 ]; then
      echo 'travis_fold:start:python3'
      pyenv global system 3.7
      python3.7 -m pip install -q virtualenv wheel six
      echo 'travis_fold:end:python3'
    elif [ "${LANGUAGE}" == dotnet ]; then
      echo 'travis_fold:start:dotnet'
      installdotnetsdk
      echo 'travis_fold:end:dotnet'
    fi
  elif [ "${TRAVIS_OS_NAME}" == osx ]; then
    echo 'travis_fold:start:c++'
    brew update
    # see https://github.com/travis-ci/travis-ci/issues/10275
    brew install gcc || brew link --overwrite gcc
    brew install make ccache
    echo 'travis_fold:end:c++'
    if [ "${LANGUAGE}" != cc ]; then
      echo 'travis_fold:start:swig'
      brew install swig
      echo 'travis_fold:end:swig'
    fi
    if [ "${LANGUAGE}" == python3 ]; then
      echo 'travis_fold:start:python3'
      # brew upgrade python
      python3 -m pip install -q virtualenv wheel six
      echo 'travis_fold:end:python3'
    elif [ "${LANGUAGE}" == dotnet ]; then
      echo 'travis_fold:start:dotnet'
      brew tap isen-ng/dotnet-sdk-versions
      brew cask install dotnet-sdk-2.2.400 dotnet-sdk-3.0.100
      echo 'travis_fold:end:dotnet'
    fi
  fi
fi

#############
##  CMAKE  ##
#############
if [ "${BUILDER}" == cmake ]; then
  if [ "${TRAVIS_OS_NAME}" == linux ]; then
    if [ "${LANGUAGE}" != cc ]; then
      echo 'travis_fold:start:swig'
      installswig
      echo 'travis_fold:end:swig'
      echo 'travis_fold:start:python3'
      if [ "${ARCH}" == "amd64" ]; then
 	pyenv global system 3.7
	python3.7 -m pip install -q virtualenv wheel six
      elif [ "${ARCH}" == "ppc64le" ]; then
	sudo apt-get install python3-dev python3-pip
	python3.5 -m pip install -q virtualenv wheel six
      elif [ "${ARCH}" == "amd64" ]; then
	sudo apt-get install python3-dev python3-pip pcre-dev
	python3 -m pip install -q virtualenv wheel six
      fi
      echo 'travis_fold:end:python3'
    fi
  elif [ "${TRAVIS_OS_NAME}" == osx ]; then
    echo 'travis_fold:start:c++'
    brew update
    # see https://github.com/travis-ci/travis-ci/issues/10275
    brew install gcc || brew link --overwrite gcc
    brew install make ccache
    echo 'travis_fold:end:c++'
    echo 'travis_fold:start:swig'
    brew install swig
    echo 'travis_fold:end:swig'
    echo 'travis_fold:start:python3'
    brew upgrade python
    echo 'travis_fold:end:python3'
  fi
fi
