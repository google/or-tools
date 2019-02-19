#!/usr/bin/env bash
# usage: ./tools/generate_all_dotnet_csproj.sh
set -e

# Gets OR_TOOLS_MAJOR and OR_TOOLS_MINOR
DIR="${BASH_SOURCE%/*}"
if [[ ! -d "${DIR}" ]]; then
  DIR="${PWD}";
fi
# shellcheck disable=SC1090
. "${DIR}/../Version.txt"

###############
##  Cleanup  ##
###############
echo "Remove previous .sln files..."
rm -f examples/*/*.sln
rm -f ortools/*/samples/*.sln
echo "Remove previous .sln files...DONE"

###########
##  SLN  ##
###########
if hash dotnet 2>/dev/null; then
  SLN=Google.OrTools.Examples.sln
  echo "Generate ${SLN}..."
  pushd examples/dotnet
  dotnet new sln -n ${SLN%.sln}
  for i in *.*proj; do
    dotnet sln ${SLN} add "$i"
  done
  echo "Generate ${SLN}...DONE"
  popd

  SLN=Google.OrTools.Contrib.sln
  echo "Generate ${SLN}..."
  pushd examples/contrib
  dotnet new sln -n ${SLN%.sln}
  for i in *.*proj; do
    dotnet sln ${SLN} add "$i"
  done
  echo "Generate ${SLN}...DONE"
  popd
fi
# vim: set tw=0 ts=2 sw=2 expandtab:
