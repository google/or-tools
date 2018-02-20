#!/usr/bin/env bash
set -x
set -e

function checkenv() {
	if [ "${BUILDER}" == make ];then
		make --version
	fi
	cmake --version
	if [ "${BUILDER}" == cmake ];then
		swig -version
		python3.6 --version
		python3.6 -m pip --version

	fi
}

################
##  MAKEFILE  ##
################
if [ "${BUILDER}" == make ];then
	if [ "${TRAVIS_OS_NAME}" == linux ];then
		if [ "${DISTRO}" == native ];then
			checkenv
			make detect
			make third_party
			make "${LANGUAGE}"
			make test_"${LANGUAGE}"
		else
			# Linux Docker Makefile build:
			echo "NOT SUPPORTED"
		fi
	elif [ "${TRAVIS_OS_NAME}" == osx ];then
		if [ "${DISTRO}" == native ];then
			checkenv
			make detect
			make third_party
			make "${LANGUAGE}"
			make test_"${LANGUAGE}"
		else
			# MacOS Docker Makefile build:
			echo "NOT SUPPORTED"
		fi
	fi
fi

#############
##  CMAKE  ##
#############
if [ "${BUILDER}" == cmake ];then
	if [ "${TRAVIS_OS_NAME}" == linux ];then
		if [ "${DISTRO}" == native ];then
			export PATH="${HOME}"/swig/bin:"${PATH}"
			pyenv global system 3.6
			checkenv
			cmake -H. -Bbuild
			cmake --build build --target all
			cmake --build build --target test -- CTEST_OUTPUT_ON_FAILURE=1
		else
			# Linux Docker CMake build:
			echo "NOT SUPPORTED"
		fi
	elif [ "${TRAVIS_OS_NAME}" == osx ];then
		if [ "${DISTRO}" == native ];then
			checkenv
			cmake -H. -Bbuild
			cmake --build build --target all
			cmake --build build --target test -- CTEST_OUTPUT_ON_FAILURE=1
		else
			# MacOS Docker CMake build:
			echo "NOT SUPPORTED"
		fi
	fi
fi
