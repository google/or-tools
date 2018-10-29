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

function installmono() {
  # Need mono >= 4.2 cf. makefiles/Makefile.port.mk:87
  # http://www.mono-project.com/download/stable/
  sudo apt-key adv \
    --keyserver hkp://keyserver.ubuntu.com:80 \
    --recv-keys 3FA7E0328081BFF6A14DA29AA6A19B38D3D831EF &&
    echo "deb http://download.mono-project.com/repo/ubuntu stable-trusty main" | \
    sudo tee /etc/apt/sources.list.d/mono-official-stable.list &&
    sudo apt-get update -qq &&
    sudo apt-get install -yqq mono-complete
}

function installdotnetsdk(){
  # Installs for Ubuntu Trusty distro
  curl https://packages.microsoft.com/keys/microsoft.asc | gpg --dearmor > microsoft.gpg &&
    sudo mv microsoft.gpg /etc/apt/trusted.gpg.d/microsoft.gpg &&
    sudo sh -c 'echo "deb [arch=amd64] https://packages.microsoft.com/repos/microsoft-ubuntu-trusty-prod trusty main" > /etc/apt/sources.list.d/dotnetdev.list' &&
    # Install dotnet sdk 2.1
  sudo apt-get install apt-transport-https &&
    sudo apt-get update -qq &&
    sudo apt-get install -yqq dotnet-sdk-2.1
}

################
##  MAKEFILE  ##
################
if [ "${BUILDER}" == make ]; then
  if [ "${TRAVIS_OS_NAME}" == linux ]; then
    sudo apt-get -qq update
    sudo apt-get -yqq install autoconf libtool zlib1g-dev gawk curl	lsb-release
    if [ "${LANGUAGE}" != cc ]; then
      installswig
    fi
    if [ "${LANGUAGE}" == python2 ]; then
      pyenv global system 2.7
      python2.7 -m pip install -q virtualenv wheel six;
    elif [ "${LANGUAGE}" == python3 ]; then
      pyenv global system 3.6
      python3.6 -m pip install -q virtualenv wheel six
    elif [ "${LANGUAGE}" == dotnet ]; then
      installdotnetsdk
    fi
  elif [ "${TRAVIS_OS_NAME}" == osx ]; then
    brew update
    # see https://github.com/travis-ci/travis-ci/issues/10275
    brew install gcc || brew link --overwrite gcc
    brew install make --with-default-names
    if [ "${LANGUAGE}" != cc ]; then
      brew install swig
    fi
    if [ "${LANGUAGE}" == python2 ]; then
      brew outdated | grep -q python@2 && brew upgrade python@2
      python2 -m pip install -q virtualenv wheel six
    elif [ "${LANGUAGE}" == python3 ]; then
      brew upgrade python
      python3 -m pip install -q virtualenv wheel six
    elif [ "${LANGUAGE}" == java ]; then
      brew tap caskroom/versions
      brew cask install java8
    elif [ "${LANGUAGE}" == dotnet ]; then
      brew tap caskroom/cask
      brew cask install dotnet-sdk
    fi
  fi
fi

#############
##  CMAKE  ##
#############
if [ "${BUILDER}" == cmake ]; then
  if [ "${TRAVIS_OS_NAME}" == linux ]; then
    installswig
    pyenv global system 3.6
    python3.6 -m pip install -q virtualenv wheel six
  elif [ "${TRAVIS_OS_NAME}" == osx ]; then
    brew update
    # see https://github.com/travis-ci/travis-ci/issues/10275
    brew install gcc || brew link --overwrite gcc
    brew install swig
    brew upgrade python
  fi
fi
