#!/usr/bin/env bash
set -x
set -e

function installswig() {
	# Need SWIG >= 3.0.8
	cd /tmp/ &&
		curl -s -J -O -k -L \
		'https://sourceforge.net/projects/swig/files/swig/swig-3.0.12/swig-3.0.12.tar.gz/download' &&
		tar zxf swig-3.0.12.tar.gz && cd swig-3.0.12 &&
		./configure --prefix "${HOME}"/swig/ 1>/dev/null &&
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

################
##  MAKEFILE  ##
################
if [ "${BUILDER}" == make ]; then
	if [ "${TRAVIS_OS_NAME}" == linux ]; then
		if [ "${DISTRO}" == native ]; then
			sudo apt-get -qq update
			sudo apt-get -yqq install autoconf libtool zlib1g-dev gawk curl	lsb-release
			if [ "${LANGUAGE}" != cc ]; then
				installswig
			fi
			if [ "${LANGUAGE}" == python ]; then
				pyenv global system 3.6;
				python3.6 -m pip install -q virtualenv wheel six;
			elif [ "${LANGUAGE}" == csharp ]; then
				installmono
			elif [ "${LANGUAGE}" == fsharp ]; then
				installmono
				sudo apt-get -yqq install fsharp
			fi
		else
			# Linux Docker Makefile build:
			echo "NOT SUPPORTED"
			exit 42
		fi
	elif [ "${TRAVIS_OS_NAME}" == osx ]; then
		if [ "${DISTRO}" == native ]; then
			if [ "${LANGUAGE}" != cc ]; then
				brew update;
				brew install swig;
			fi
			if [ "${LANGUAGE}" == python ]; then
				brew install python3;
				python3.6 -m pip install -q virtualenv wheel six;
			elif [ "${LANGUAGE}" == java ]; then
				brew cask install java;
			elif [ "${LANGUAGE}" == csharp ] || [ "${LANGUAGE}" == fsharp ]; then
				brew install mono;
			fi
		else
			# MacOS Docker Makefile build:
			echo "NOT SUPPORTED"
			exit 42
		fi
	fi
fi

#############
##  CMAKE  ##
#############
if [ "${BUILDER}" == cmake ]; then
	if [ "${TRAVIS_OS_NAME}" == linux ]; then
		if [ "${DISTRO}" == native ]; then
			installswig
		else
			# Linux Docker CMake build:
			echo "NOT SUPPORTED"
			exit 42
		fi
	elif [ "${TRAVIS_OS_NAME}" == osx ]; then
		if [ "${DISTRO}" == native ]; then
			brew update;
			brew install swig;
			brew install python3;
		else
			# MacOS Docker CMake build:
			echo "NOT SUPPORTED"
			exit 42
		fi
	fi
fi
