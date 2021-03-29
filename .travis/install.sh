#!/usr/bin/env bash
set -x
set -e

function installswig() {
  # Need SWIG >= 4.0.0
  cd /tmp/ &&
    wget https://github.com/swig/swig/archive/rel-4.0.2.tar.gz &&
    tar zxf rel-4.0.2.tar.gz && cd swig-rel-4.0.2 &&
    ./autogen.sh && ./configure --prefix "${HOME}"/swig/ 1>/dev/null &&
    make >/dev/null &&
    make install >/dev/null
}

function installdotnetsdk(){
  # Installs for Ubuntu Bionic distro
  # see https://docs.microsoft.com/fr-fr/dotnet/core/install/linux#ubuntu
  sudo apt-get update -qq
  sudo apt-get install -yq apt-transport-https dpkg
  wget -q https://packages.microsoft.com/config/ubuntu/18.04/packages-microsoft-prod.deb
  sudo dpkg -i packages-microsoft-prod.deb
  # Install dotnet-sdk 3.1
  sudo apt-get update -qq
  sudo apt-get install -yqq dotnet-sdk-3.1
}

function installcmake(){
  # Install CMake 3.18.5
  wget "https://cmake.org/files/v3.18/cmake-3.18.5-Linux-x86_64.sh"
  chmod a+x cmake-3.18.5-Linux-x86_64.sh
  sudo ./cmake-3.18.5-Linux-x86_64.sh --prefix=/usr/local/ --skip-license
  rm cmake-3.18.5-Linux-x86_64.sh
}

################
##  MAKEFILE  ##
################
if [ "${BUILDER}" == make ]; then
  echo "TRAVIS_OS_NAME = ${TRAVIS_OS_NAME}"
  if [ "${TRAVIS_OS_NAME}" == linux ]; then
    echo 'travis_fold:start:c++'
    sudo apt-get -qq update
    sudo apt-get -yqq install autoconf libtool zlib1g-dev gawk curl lsb-release
    installcmake
    echo 'travis_fold:end:c++'
    if [ "${LANGUAGE}" != cc ]; then
      echo 'travis_fold:start:swig'
      installswig
      echo 'travis_fold:end:swig'
    fi
    if [ "${LANGUAGE}" == python3 ]; then
      echo 'travis_fold:start:python3'
      pyenv global system 3.7
      python3.7 -m pip install -q virtualenv wheel absl-py mypy-protobuf
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
      python3 -m pip install -q virtualenv wheel absl-py mypy-protobuf
      echo 'travis_fold:end:python3'
    elif [ "${LANGUAGE}" == dotnet ]; then
      echo 'travis_fold:start:dotnet'
      brew cask install dotnet-sdk
      echo 'travis_fold:end:dotnet'
    fi
  fi
fi

#############
##  CMAKE  ##
#############
if [ "${BUILDER}" == cmake ]; then
  if [ "${TRAVIS_OS_NAME}" == linux ]; then
    installcmake
    if [ "${LANGUAGE}" != cc ]; then
      echo 'travis_fold:start:swig'
      installswig
      echo 'travis_fold:end:swig'
      echo 'travis_fold:start:python3'
      if [ "${ARCH}" == "amd64" ]; then
        pyenv global system 3.7
        python3.7 -m pip install -q virtualenv wheel absl-py mypy-protobuf
      elif [ "${ARCH}" == "ppc64le" ]; then
        sudo apt-get install python3-dev python3-pip
        python3.5 -m pip install -q virtualenv wheel absl-py mypy-protobuf
      elif [ "${ARCH}" == "amd64" ]; then
        sudo apt-get install python3-dev python3-pip pcre-dev
        python3 -m pip install -q virtualenv wheel absl-py mypy-protobuf
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
    # echo 'travis_fold:start:python3'
    # brew upgrade python
    # echo 'travis_fold:end:python3'
  fi
fi
