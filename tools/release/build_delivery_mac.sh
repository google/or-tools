#!/usr/bin/env bash
set -eo pipefail

function help() {
  local -r NAME=$(basename "$0")
  local -r BOLD="\e[1m"
  local -r RESET="\e[0m"
  local -r help=$(cat << EOF
${BOLD}NAME${RESET}
\t$NAME - Build delivery using the ${BOLD}local host system${RESET}.
${BOLD}SYNOPSIS${RESET}
\t$NAME [-h|--help] [examples|dotnet|java|python|all|reset]
${BOLD}DESCRIPTION${RESET}
\tBuild Google OR-Tools deliveries.
\tYou ${BOLD}MUST${RESET} define the following variables before running this script:
\t* ORTOOLS_TOKEN: secret use to decrypt key to sign .Net and Java package.

${BOLD}OPTIONS${RESET}
\t-h --help: display this help text
\tdotnet: build all .Net packages
\tjava: build all Java packages
\tpython: build all Pyhon packages
\texamples: build examples archives
\tall: build everything (default)

${BOLD}EXAMPLES${RESET}
Using export to define the ${BOLD}ORTOOLS_TOKEN${RESET} env and only building the Java packages:
export ORTOOLS_TOKEN=SECRET
$0 java

note: the 'export ...' should be placed in your bashrc to avoid any leak
of the secret in your bash history
EOF
)
  echo -e "$help"
}

function assert_defined(){
  if [[ -z "${!1}" ]]; then
    >&2 echo "Variable '${1}' must be defined"
    exit 1
  fi
}

function build_cxx() {
  if echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" | cmp --silent "${ROOT_DIR}/export/cxx_build" -; then
    echo "build C++ up to date!" | tee -a build.log
    return 0
  fi

  # Check all prerequisite
  command -v clang
  command -v clang | xargs echo "clang: " | tee -a build.log
  command -v cmake
  command -v cmake | xargs echo "cmake: " | tee -a build.log
  command -v make
  command -v make | xargs echo "make: " | tee -a build.log

  # Clean everything
  cd "${ROOT_DIR}" || exit 2
  make clean
  make clean_third_party

  #  Build Third Party
  echo -n "Build Third Party..." | tee -a build.log
  make third_party UNIX_PYTHON_VER=3.9
  echo "DONE" | tee -a build.log

  echo -n "Build C++..." | tee -a build.log
  make cc -l 4 UNIX_PYTHON_VER=3.9
  echo "DONE" | tee -a build.log
  #make test_cc -l 4 UNIX_PYTHON_VER=3.9
  #echo "make test_cc: DONE" | tee -a build.log

  echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" > "${ROOT_DIR}/export/cxx_build"
}

# .Net build
function build_dotnet() {
  if echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" | cmp --silent "${ROOT_DIR}/export/dotnet_build" -; then
    echo "build .Net up to date!" | tee -a build.log
    return 0
  fi
  build_cxx

  command -v swig
  command -v swig | xargs echo "swig: " | tee -a build.log
  command -v dotnet
  command -v dotnet | xargs echo "dotnet: " | tee -a build.log

  # Install .Net SNK
  echo -n "Install .Net SNK..." | tee -a build.log
  openssl aes-256-cbc -iter 42 -pass pass:"$ORTOOLS_TOKEN" \
    -in "${RELEASE_DIR}/or-tools.snk.enc" \
    -out "${ROOT_DIR}/export/or-tools.snk" -d
  DOTNET_SNK=export/or-tools.snk
  echo "DONE" | tee -a build.log

  # Clean dotnet
  cd "${ROOT_DIR}" || exit 2
  make clean_dotnet

  echo -n "Build .Net..." | tee -a build.log
  make dotnet -l 4 UNIX_PYTHON_VER=3.9
  echo "DONE" | tee -a build.log
  #make test_dotnet -l 4 UNIX_PYTHON_VER=3.9
  #echo "make test_dotnet: DONE" | tee -a build.log

  cp temp_dotnet/packages/*nupkg export/
  echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" > "${ROOT_DIR}/export/dotnet_build"
}

# Java build
function build_java() {
  if echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" | cmp --silent "${ROOT_DIR}/export/java_build" -; then
    echo "build Java up to date!" | tee -a build.log
    return 0
  fi
  build_cxx

  command -v swig
  command -v swig | xargs echo "swig: " | tee -a build.log
  # maven require JAVA_HOME
  if [[ -z "${JAVA_HOME}" ]]; then
    echo "JAVA_HOME: not found !" | tee -a build.log
    exit 1
  else
    echo "JAVA_HOME: ${JAVA_HOME}" | tee -a build.log
    command -v java
    command -v java | xargs echo "java: " | tee -a build.log
    command -v javac
    command -v javac | xargs echo "javac: " | tee -a build.log
    command -v jar
    command -v jar | xargs echo "jar: " | tee -a build.log
    command -v mvn
    command -v mvn | xargs echo "mvn: " | tee -a build.log
    java -version 2>&1 | head -n 1 | grep 1.8
  fi
  # Maven central need gpg sign and we store the release key encoded using openssl
  command -v openssl
  command -v openssl | xargs echo "openssl: " | tee -a build.log
  command -v gpg
  command -v gpg | xargs echo "gpg: " | tee -a build.log

  # Install Java GPG
  echo -n "Install Java GPG..." | tee -a build.log
  openssl aes-256-cbc -iter 42 -pass pass:"$ORTOOLS_TOKEN" \
  -in tools/release/private-key.gpg.enc \
  -out private-key.gpg -d
  gpg --batch --import private-key.gpg
  # Don't need to trust the key
  #expect -c "spawn gpg --edit-key "corentinl@google.com" trust quit; send \"5\ry\r\"; expect eof"

  # Install the maven settings.xml having the GPG passphrase
  mkdir -p ~/.m2
  openssl aes-256-cbc -iter 42 -pass pass:"$ORTOOLS_TOKEN" \
  -in tools/release/settings.xml.enc \
  -out ~/.m2/settings.xml -d
  echo "DONE" | tee -a build.log

  # Clean java
  cd "${ROOT_DIR}" || exit 2
  make clean_java

  echo -n "Build Java..." | tee -a build.log
  make java -l 4 UNIX_PYTHON_VER=3.9
  echo "DONE" | tee -a build.log
  #make test_java -l 4 UNIX_PYTHON_VER=3.9
  #echo "make test_java: DONE" | tee -a build.log

  cp temp_java/ortools-darwin-x86-64/target/*.jar* export/
  cp temp_java/ortools-java/target/*.jar* export/
  echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" > "${ROOT_DIR}/export/java_build"
}

function build_fz() {
  if echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" | cmp --silent "${ROOT_DIR}/export/fz_build" -; then
    echo "build Flatzinc up to date!" | tee -a build.log
    return 0
  fi
  build_cxx

  # Clean fz
  cd "${ROOT_DIR}" || exit 2
  make clean_fz

  echo -n "Build flatzinc..." | tee -a build.log
  make fz -l 4 UNIX_PYTHON_VER=3.9
  echo "DONE" | tee -a build.log

  echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" > "${ROOT_DIR}/export/fz_build"
}

# Create Archive
function build_archive() {
  if echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" | cmp --silent "${ROOT_DIR}/export/archive_build" -; then
    echo "build archive up to date!" | tee -a build.log
    return 0
  fi
  build_fz

  # Clean archive
  cd "${ROOT_DIR}" || exit 2
  make clean_archive

  echo -n "Make archive..." | tee -a build.log
  make archive UNIX_PYTHON_VER=3.9
  echo "DONE" | tee -a build.log
  echo -n "Test archive..." | tee -a build.log
  make test_archive UNIX_PYTHON_VER=3.9
  echo "DONE" | tee -a build.log

  echo -n "Make flatzinc archive..." | tee -a build.log
  make fz_archive UNIX_PYTHON_VER=3.9
  echo "DONE" | tee -a build.log
  echo -n "Test flatzinc archive..." | tee -a build.log
  make test_fz_archive UNIX_PYTHON_VER=3.9
  echo "DONE" | tee -a build.log

  mv or-tools_*.tar.gz export/
  echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" > "${ROOT_DIR}/export/archive_build"
}

# Build Examples
function build_examples() {
  if echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" | cmp --silent "${ROOT_DIR}/export/examples_build" -; then
    echo "build examples up to date!" | tee -a build.log
    return 0
  fi

  cd "${ROOT_DIR}" || exit 2

  rm -rf temp ./*.tar.gz
  echo -n "Build examples archives..." | tee -a build.log
  echo -n "  C++ examples archive..." | tee -a build.log
  make cc_examples_archive UNIX_PYTHON_VER=3.9
  echo -n "  Python examples archive..." | tee -a build.log
  make python_examples_archive UNIX_PYTHON_VER=3.9
  echo -n "  Java examples archive..." | tee -a build.log
  make java_examples_archive UNIX_PYTHON_VER=3.9
  echo -n "  .Net examples archive..." | tee -a build.log
  make dotnet_examples_archive UNIX_PYTHON_VER=3.9
  echo "DONE" | tee -a build.log

  mv or-tools_*_examples_*.tar.gz export/
  echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" > "${ROOT_DIR}/export/examples_build"
}

# Python 3
# todo(mizux) Use `make --directory tools/docker python` instead
function build_python() {
  build_cxx

  command -v swig
  command -v swig | xargs echo "swig: " | tee -a build.log
  PY=(3.6 3.7 3.8 3.9)
  for i in "${PY[@]}"; do
    command -v "python$i"
    command -v "python$i" | xargs echo "python$i: " | tee -a build.log
    "python$i" -c "import distutils.util as u; print(u.get_platform())" | tee -a build.log
    "python$i" -m pip install --user wheel absl-py mypy-protobuf
  done
  command -v protoc-gen-mypy | xargs echo "protoc-gen-mypy: " | tee -a build.log

  for i in "${PY[@]}"; do
    echo -n "Cleaning Python 3..." | tee -a build.log
    make clean_python UNIX_PYTHON_VER="$i"
    echo "DONE" | tee -a build.log

    echo -n "Build Python $i..." | tee -a build.log
    make python -l 4 UNIX_PYTHON_VER="$i"
    echo "DONE" | tee -a build.log
    #make test_python UNIX_PYTHON_VER=$i
    #echo "make test_python$i: DONE" | tee -a build.log
    echo -n "Build Python $i wheel archive..." | tee -a build.log
    make package_python UNIX_PYTHON_VER="$i"
    echo "DONE" | tee -a build.log
    echo -n "Test Python $i wheel archive..." | tee -a build.log
    make test_package_python UNIX_PYTHON_VER="$i"
    echo "DONE" | tee -a build.log

    cp "temp_python$i"/ortools/dist/*.whl export/
  done
  echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" > "${ROOT_DIR}/export/python_build"
}

# Main
function main() {
  case ${1} in
    -h | --help)
      help; exit ;;
  esac

  assert_defined ORTOOLS_TOKEN
  echo "ORTOOLS_TOKEN: FOUND" | tee build.log
  make print-OR_TOOLS_VERSION | tee -a build.log

  local -r ROOT_DIR="$(cd -P -- "$(dirname -- "$0")/../.." && pwd -P)"
  echo "ROOT_DIR: '${ROOT_DIR}'" | tee -a build.log

  local -r RELEASE_DIR="$(cd -P -- "$(dirname -- "$0")" && pwd -P)"
  echo "RELEASE_DIR: '${RELEASE_DIR}'" | tee -a build.log

  local -r ORTOOLS_BRANCH=$(git rev-parse --abbrev-ref HEAD)
  local -r ORTOOLS_SHA1=$(git rev-parse --verify HEAD)

  mkdir -p export
  case ${1} in
    cxx|dotnet|java|python|archive|examples)
      "build_$1"
      exit ;;
    all)
      build_dotnet
      build_java
      #build_python
      build_archive
      build_examples
      exit ;;
    *)
      >&2 echo "Target '${1}' unknown" | tee -a build.log
      exit 1
  esac
  exit 0
}

main "${1:-all}"

