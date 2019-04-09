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
    echo 'travis_fold:start:env'
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
    echo 'travis_fold:end:env'
    echo 'travis_fold:start:third_party'
    make third_party --jobs=4
    echo 'travis_fold:end:third_party'
    if [ "${LANGUAGE}" == python2 ] || [ "${LANGUAGE}"  == python3 ]; then
      echo 'travis_fold:start:python'
      make python --jobs=4
      echo 'travis_fold:end:python'
      echo 'travis_fold:start:test_python'
      make test_python --jobs=4
      echo 'travis_fold:end:test_python'
    elif [ "${LANGUAGE}" == java ]; then
      echo 'travis_fold:start:java'
      make java --jobs=4
      echo 'travis_fold:end:java'
      echo 'travis_fold:start:test_java'
      make test_java --jobs=1
      echo 'travis_fold:end:test_java'
    else
      echo "travis_fold:start:${LANGUAGE}"
      make "${LANGUAGE}" --jobs=4
      echo "travis_fold:end:${LANGUAGE}"
      echo "travis_fold:start:test_${LANGUAGE}"
      make test_"${LANGUAGE}" --jobs=4
      echo "travis_fold:end:test_${LANGUAGE}"
    fi
    if [ "${LANGUAGE}" == cc ]; then
      echo "travis_fold:start:flatzinc"
      make test_fz --jobs=2
      echo "travis_fold:end:flatzinc"
    fi
  elif [ "${TRAVIS_OS_NAME}" == osx ];then
    echo 'travis_fold:start:env'
    export PATH="/usr/local/opt/ccache/libexec:$PATH"
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
    echo 'travis_fold:end:env'
    echo 'travis_fold:start:third_party'
    make third_party --jobs=4
    echo 'travis_fold:end:third_party'
    if [ "${LANGUAGE}" == python2 ] || [ "${LANGUAGE}"  == python3 ]; then
      echo 'travis_fold:start:python'
      make python --jobs=4
      echo 'travis_fold:end:python'
      echo 'travis_fold:start:test_python'
      make test_python --jobs=4
      echo 'travis_fold:end:test_python'
    elif [ "${LANGUAGE}" == java ]; then
      echo 'travis_fold:start:java'
      make java --jobs=4
      echo 'travis_fold:end:java'
      echo 'travis_fold:start:test_java'
      make test_java --jobs=1
      echo 'travis_fold:end:test_java'
    else
      echo "travis_fold:start:${LANGUAGE}"
      make "${LANGUAGE}" --jobs=4
      echo "travis_fold:end:${LANGUAGE}"
      echo "travis_fold:start:test_${LANGUAGE}"
      make test_"${LANGUAGE}" --jobs=4
      echo "travis_fold:end:test_${LANGUAGE}"
    fi
    if [ "${LANGUAGE}" == cc ]; then
      echo "travis_fold:start:flatzinc"
      make test_fz --jobs=2
      echo "travis_fold:end:flatzinc"
    fi
  fi
fi

#############
##  CMAKE  ##
#############
if [ "${BUILDER}" == cmake ];then
  export CMAKE_BUILD_PARALLEL_LEVEL=4
  if [ "${TRAVIS_OS_NAME}" == linux ];then
    echo 'travis_fold:start:env'
    # Add clang support in ccache
    if [[ "${CC}" == "clang" ]]; then
      sudo ln -s ../../bin/ccache /usr/lib/ccache/clang
      export CFLAGS="-Qunused-arguments $CFLAGS"
    fi
    if [[ "${CXX}" == "clang++" ]]; then
      sudo ln -s ../../bin/ccache /usr/lib/ccache/clang++
      export CXXFLAGS="-Qunused-arguments $CXXFLAGS"
    fi
    export PATH="${HOME}/swig/bin:${PATH}"
    pyenv global system 3.7
    checkenv
    echo 'travis_fold:end:env'
  elif [ "${TRAVIS_OS_NAME}" == osx ];then
    echo 'travis_fold:start:env'
    export PATH="/usr/local/opt/ccache/libexec:$PATH"
    checkenv
    echo 'travis_fold:end:env'
  fi

  echo 'travis_fold:start:configure'
  cmake -H. -Bbuild -DBUILD_DEPS:BOOL=ON
  echo 'travis_fold:end:configure'

  echo 'travis_fold:start:build'
  cmake --build build --target all
  echo 'travis_fold:end:build'

  echo 'travis_fold:start:test'
  cmake --build build --target test -- CTEST_OUTPUT_ON_FAILURE=1
  echo 'travis_fold:end:test'
fi

#############
##  BAZEL  ##
#############
if [ "${BUILDER}" == bazel ]; then
  echo 'travis_fold:start:build'
  bazel build --curses=no --copt='-Wno-sign-compare' //...:all
  echo 'travis_fold:end:build'

  echo 'travis_fold:start:test'
  bazel test -c opt --curses=no --copt='-Wno-sign-compare' //...:all
  echo 'travis_fold:end:test'
fi
