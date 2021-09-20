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

mkdir -p "${DIR}/../temp_dotnet/packages"
###############
##  Cleanup  ##
###############
echo "Remove previous .[cf]sproj files..."
rm -f examples/*/*.csproj
rm -f examples/*/*.fsproj
rm -f ortools/*/samples/*.csproj
rm -f ortools/*/samples/*.fsproj
echo "Remove previous .[cf]sproj files...DONE"

################
##  Examples  ##
################
for FILE in examples/*/*.[cf]s ; do
  # if no files found do nothing
  [[ -e "$FILE" ]] || continue
  echo "Generating ${FILE}proj..."
  ./tools/generate_dotnet_proj.sh "$FILE"
done
###############
##  Samples  ##
###############
for FILE in ortools/*/samples/*.[cf]s ; do
  # if no files found do nothing
  [[ -e "$FILE" ]] || continue
  echo "Generating ${FILE}proj..."
  ./tools/generate_dotnet_proj.sh "$FILE"
done
# vim: set tw=0 ts=2 sw=2 expandtab:
