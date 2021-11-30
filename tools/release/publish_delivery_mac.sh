#!/usr/bin/env bash
set -eo pipefail

function help() {
  local -r NAME=$(basename "$0")
  local -r BOLD="\e[1m"
  local -r RESET="\e[0m"
  local -r help=$(cat << EOF
${BOLD}NAME${RESET}
\t$NAME - Publish delivery using the ${BOLD}local host system${RESET}.
${BOLD}SYNOPSIS${RESET}
\t$NAME [-h|--help] [java|python]
${BOLD}DESCRIPTION${RESET}
\tPublish Google OR-Tools deliveries.
\tYou ${BOLD}MUST${RESET} define the following variables before running this script:
\t* ORTOOLS_TOKEN: secret use to decrypt key to sign dotnet and java package.

${BOLD}OPTIONS${RESET}
\t-h --help: display this help text
\tjava: publish the Java runtime packages
\tpython: publish all Pyhon packages
\tall: publish everything (default)

${BOLD}EXAMPLES${RESET}
Using export to define the ${BOLD}ORTOOLS_TOKEN${RESET} env and only publishing the Java packages:
export ORTOOLS_TOKEN=SECRET
$0 java

note: the 'export ORTOOLS_TOKEN=...' should be placed in your bashrc to avoid any leak
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

# Java publish
function publish_java() {
  if echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" | cmp --silent "${ROOT_DIR}/export/java_publish" -; then
    echo "publish Java up to date!"
    return 0
  fi

  # maven require JAVA_HOME
  if [[ -z "${JAVA_HOME}" ]]; then
    echo "JAVA_HOME: not found !" | tee publish.log
    exit 1
  else
    echo "JAVA_HOME: ${JAVA_HOME}" | tee -a publish.log
    command -v mvn
    command -v mvn | xargs echo "mvn: " | tee -a publish.log
    java -version | tee -a publish.log
    java -version 2>&1 | head -n 1 | grep 1.8
  fi
  # Maven central need gpg sign and we store the release key encoded using openssl
  command -v openssl
  command -v openssl | xargs echo "openssl: " | tee -a publish.log
  command -v gpg
  command -v gpg | xargs echo "gpg: " | tee -a publish.log

  echo -n "Publish Java..." | tee -a publish.log
  make publish_java_runtime -l 4 UNIX_PYTHON_VER=3.9
  echo "DONE" | tee -a publish.log

  echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" > "${ROOT_DIR}/export/java_publish"
}

# Python publish
function publish_python() {
  if echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" | cmp --silent "${ROOT_DIR}/export/python_publish" -; then
    echo "publish Python up to date!"
    return 0
  fi

  command -v twine
  command -v twine | xargs echo "twine: " | tee -a publish.log

  echo -n "Publish Python..." | tee -a publish.log
  pushd export
  twine upload "*.whl"
  popd
  echo "DONE" | tee -a publish.log

  echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" > "${ROOT_DIR}/export/python_publish"
}

# Main
function main() {
  case ${1} in
    -h | --help)
      help; exit ;;
  esac

  assert_defined ORTOOLS_TOKEN
  echo "ORTOOLS_TOKEN: FOUND" | tee publish.log
  make print-OR_TOOLS_VERSION | tee -a publish.log

  local -r ROOT_DIR="$(cd -P -- "$(dirname -- "$0")/../.." && pwd -P)"
  echo "ROOT_DIR: '${ROOT_DIR}'"

  local -r RELEASE_DIR="$(cd -P -- "$(dirname -- "$0")" && pwd -P)"
  echo "RELEASE_DIR: '${RELEASE_DIR}'"

  local -r ORTOOLS_BRANCH=$(git rev-parse --abbrev-ref HEAD)
  local -r ORTOOLS_SHA1=$(git rev-parse --verify HEAD)

  mkdir -p export
  case ${1} in
    java|python)
      "publish_$1"
      exit ;;
    all)
      publish_java
      publish_python
      exit ;;
    *)
      >&2 echo "Target '${1}' unknown"
      exit 1
  esac
  exit 0
}

main "${1:-all}"

