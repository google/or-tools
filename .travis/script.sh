#!/usr/bin/env bash
set -x
set -e

function checkenv() {
	if [ "${BUILDER}" == make ];then
		make --version
	fi
	cmake --version
	if [ "${BUILDER}" == cmake ] || [ "${LANGUAGE}" != cc ]; then
		swig -version
	fi
	if [ "${BUILDER}" == cmake ] || [ "${LANGUAGE}" == python ];then
		python3.6 --version
		python3.6 -m pip --version
	elif [ "${LANGUAGE}" == java ]; then
		java -version
	elif [ "${LANGUAGE}" == csharp ]; then
		mono --version
		which csharp
	elif [ "${LANGUAGE}" == fsharp ]; then
		mono --version
		dotnet -h
	fi
}

################
##  MAKEFILE  ##
################
if [ "${BUILDER}" == make ];then
	if [ "${TRAVIS_OS_NAME}" == linux ];then
		if [ "${DISTRO}" == native ];then
			if [ "${LANGUAGE}" != cc ]; then
				export PATH="${HOME}"/swig/bin:"${PATH}"
			fi
			checkenv
			if [ "${LANGUAGE}" == cc ]; then
				make detect
			elif [ "${LANGUAGE}" == python ]; then
				make detect UNIX_PYTHON_VER=3.6
			elif [ "${LANGUAGE}" == java ]; then
				make detect JAVA_HOME=/usr/lib/jvm/java-8-openjdk-amd64
			elif [ "${LANGUAGE}" == csharp ] || [ "${LANGUAGE}" == fsharp ] ; then
				make detect
			fi
			cat Makefile.local
			make third_party
			make "${LANGUAGE}"
			make test_"${LANGUAGE}"
		else
			# Linux Docker Makefile build:
			echo "NOT SUPPORTED"
		fi
	elif [ "${TRAVIS_OS_NAME}" == osx ];then
		if [ "${DISTRO}" == native ];then
			if [ "${LANGUAGE}" == fsharp ]; then
				# Installer changes path but won't be picked up in current terminal session
				# Need to explicitly add location
				export PATH=/usr/local/share/dotnet:"${PATH}"
			fi
			checkenv
			if [ "${LANGUAGE}" == cc ]; then
				make detect
			elif [ "${LANGUAGE}" == python ]; then
				make detect UNIX_PYTHON_VER=3.6
			elif [ "${LANGUAGE}" == java ] || [ "${LANGUAGE}" == csharp ] || [ "${LANGUAGE}" == fsharp ]; then
				make detect
			fi
			cat Makefile.local
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
