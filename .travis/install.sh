#!/usr/bin/env bash
set -x
set -e

################
##  MAKEFILE  ##
################
if [ "${BUILDER}" == make ];then
	if [ "${TRAVIS_OS_NAME}" == linux ];then
		if [ "${DISTRO}" == native ];then
			sudo apt-get -qq update;
			sudo apt-get -yqq install \
				git autoconf libtool zlib1g-dev gawk g++ curl cmake make lsb-release;
			if [ "${LANGUAGE}" != cc ];then
				# Need SWIG >= 3.0
				cd /tmp/ &&	curl -s -J -O -k -L \
					'https://sourceforge.net/projects/swig/files/swig/swig-3.0.12/swig-3.0.12.tar.gz/download' && \
					tar zxf swig-3.0.12.tar.gz && cd swig-3.0.12 && \
					./configure --prefix "${HOME}"/swig/ 1>/dev/null && make 1>/dev/null && make install 1>/dev/null;
			fi
			if [ "${LANGUAGE}" == python ];then
				pyenv global system 3.6;
				python3.6 -m pip install -q virtualenv wheel six;
			elif [ "${LANGUAGE}" == csharp ];then
				sudo apt-get -yqq install mono-complete;
			fi
		else
			# Linux Docker Makefile build:
			echo "NOT SUPPORTED"
			exit 42
		fi
	elif [ "${TRAVIS_OS_NAME}" == osx ];then
		if [ "${DISTRO}" == native ];then
			if [ "${LANGUAGE}" != cc ]; then
				brew update;
				brew install swig;
			fi
			if [ "${LANGUAGE}" == python ];then
				brew install python3;
				python3.6 -m pip install -q virtualenv wheel six;
			elif [ "${LANGUAGE}" == java ];then
				brew cask install java;
			elif [ "${LANGUAGE}" == csharp ];then
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
if [ "${BUILDER}" == cmake ];then
	if [ "${TRAVIS_OS_NAME}" == linux ];then
		if [ "${DISTRO}" == native ];then
			# SWIG install
			cd /tmp/ &&	curl -s -J -O -k -L \
				'https://sourceforge.net/projects/swig/files/swig/swig-3.0.12/swig-3.0.12.tar.gz/download' && \
				tar zxf swig-3.0.12.tar.gz && cd swig-3.0.12 && \
				./configure --prefix "${HOME}"/swig/ 1>/dev/null && make 1>/dev/null && make install 1>/dev/null;
		else
			# Linux Docker CMake build:
			echo "NOT SUPPORTED"
			exit 42
		fi
	elif [ "${TRAVIS_OS_NAME}" == osx ];then
		if [ "${DISTRO}" == native ];then
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
