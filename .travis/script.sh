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
	if [ "${BUILDER}" == cmake ] || [ "${LANGUAGE}" == python3 ];then
    python3.7 --version
    python3.7 -m pip --version
	elif [ "${LANGUAGE}" == python2 ]; then
		python2.7 --version
		python2.7 -m pip --version
	elif [ "${LANGUAGE}" == java ]; then
		java -version
	elif [ "${LANGUAGE}" == dotnet ]; then
    dotnet --info
	fi
}

################
##  MAKEFILE  ##
################
if [ "${BUILDER}" == make ];then
  if [ "${TRAVIS_OS_NAME}" == linux ];then
    if [ "${LANGUAGE}" != cc ]; then
      export PATH="${HOME}"/swig/bin:"${PATH}"
    fi
    checkenv
    if [ "${LANGUAGE}" == cc ]; then
      make detect
    elif [ "${LANGUAGE}" == python2 ]; then
      make detect UNIX_PYTHON_VER=2.7
    elif [ "${LANGUAGE}" == python3 ]; then
      make detect UNIX_PYTHON_VER=3.7
    elif [ "${LANGUAGE}" == java ]; then
      make detect JAVA_HOME=/usr/lib/jvm/java-8-openjdk-amd64
    elif [ "${LANGUAGE}" == dotnet ] ; then
      make detect
    fi
    cat Makefile.local
    make third_party --jobs=4
    if [ "${LANGUAGE}" == python2 ] || [ "${LANGUAGE}"  == python3 ]; then
      make python --jobs=4
      make test_python --jobs=4
    elif [ "${LANGUAGE}" == java ]; then
      make java --jobs=4
      make test_java --jobs=1
    else
      make "${LANGUAGE}" --jobs=4
      make test_"${LANGUAGE}" --jobs=4
    fi
    if [ "${LANGUAGE}" == cc ]; then
      make test_fz --jobs=2
    fi
  elif [ "${TRAVIS_OS_NAME}" == osx ];then
    if [ "${LANGUAGE}" == dotnet ]; then
      # Installer changes path but won't be picked up in current terminal session
      # Need to explicitly add location
      export PATH=/usr/local/share/dotnet:"${PATH}"
    fi
    checkenv
    if [ "${LANGUAGE}" == cc ]; then
      make detect
    elif [ "${LANGUAGE}" == python2 ]; then
      make detect UNIX_PYTHON_VER=2.7
    elif [ "${LANGUAGE}" == python3 ]; then
      make detect UNIX_PYTHON_VER=3.7
    elif [ "${LANGUAGE}" == java ] || [ "${LANGUAGE}" == dotnet ] ; then
      make detect
    fi
    cat Makefile.local
    make third_party --jobs=4
    if [ "${LANGUAGE}" == python2 ] || [ "${LANGUAGE}"  == python3 ]; then
      make python --jobs=4
      make test_python --jobs=4
    elif [ "${LANGUAGE}" == java ]; then
      make java --jobs=4
      make test_java --jobs=1
    else
      make "${LANGUAGE}" --jobs=4
      make test_"${LANGUAGE}" --jobs=4
    fi
    if [ "${LANGUAGE}" == cc ]; then
      make test_fz --jobs=2
    fi
  fi
fi

#############
##  CMAKE  ##
#############
if [ "${BUILDER}" == cmake ];then
  if [ "${TRAVIS_OS_NAME}" == linux ];then
    export PATH="${HOME}"/swig/bin:"${PATH}"
    pyenv global system 3.7
    checkenv
    cmake -H. -Bbuild || true
    cmake --build build --target all -- --jobs=4
    cmake --build build --target test -- CTEST_OUTPUT_ON_FAILURE=1
  elif [ "${TRAVIS_OS_NAME}" == osx ];then
    checkenv
    cmake -H. -Bbuild || true
    cmake --build build --target all -- --jobs=4
    cmake --build build --target test -- CTEST_OUTPUT_ON_FAILURE=1
  fi
fi

if [ "${BUILDER}" == bazel ]; then
  bazel build --curses=no //...:all
  bazel test -c opt --curses=no //...:all
fi
