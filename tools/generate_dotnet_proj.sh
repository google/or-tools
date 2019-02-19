#!/usr/bin/env bash
# usage: ./tools/generate_dotnet_proj.sh ortools/sat/samples/SimpleSatProgram.cs
set -e

declare -r FILE="${1}"
[[ -e "$FILE" ]] || exit 128

declare -r FILE_PROJ="${FILE}proj";
# shellcheck disable=SC2155
declare -r SRC=$(basename "$FILE")
# shellcheck disable=SC2155
declare -r PACKAGES_PATH=$(realpath --relative-to="${FILE%/*}" packages)
if [[ $FILE == *.cs ]] ; then
  declare -r LANG_VERSION="    <LangVersion>7.2</LangVersion>"
  declare -r OR_TOOLS_PKG="Google.OrTools"
else
  declare -r LANG_VERSION=""
  declare -r OR_TOOLS_PKG="Google.OrTools.FSharp"
fi
declare -r ASSEMBLY_NAME="<AssemblyName>${OR_TOOLS_PKG}.${SRC%.*}</AssemblyName>"

# Gets OR_TOOLS_MAJOR and OR_TOOLS_MINOR
declare DIR="${BASH_SOURCE%/*}"
if [[ ! -d "${DIR}" ]]; then
  DIR="${PWD}";
fi
# shellcheck disable=SC1090
. "${DIR}/../Version.txt"

# Manage PackageReference(s)
declare DEPS
if [[ $(dirname "$FILE") == examples/tests ]] ; then
  declare -r PACKABLE="<IsPackable>false</IsPackable>"
  DEPS=$(cat <<EOF
    <PackageReference Include="Microsoft.NET.Test.Sdk" Version="15.7.0" />
    <PackageReference Include="xunit" Version="2.3.1" />
    <PackageReference Include="xunit.runner.visualstudio" Version="2.3.1" />
    <PackageReference Include="${OR_TOOLS_PKG}" Version="${OR_TOOLS_MAJOR}.${OR_TOOLS_MINOR}*" />
EOF
)
else
  declare -r PACKABLE="<IsPackable>true</IsPackable>"
  DEPS=$(cat <<EOF
    <PackageReference Include="${OR_TOOLS_PKG}" Version="${OR_TOOLS_MAJOR}.${OR_TOOLS_MINOR}*" />
EOF
)
fi

# Generate the .[cf]sproj file
cat >"$FILE_PROJ" <<EOL
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <OutputType>Exe</OutputType>
${LANG_VERSION}
    <TargetFramework>netcoreapp2.1</TargetFramework>
    <EnableDefaultItems>false</EnableDefaultItems>
    <RestoreSources>${PACKAGES_PATH};\$(RestoreSources);https://api.nuget.org/v3/index.json</RestoreSources>
    ${ASSEMBLY_NAME}
    ${PACKABLE}
  </PropertyGroup>

  <PropertyGroup Condition=" '\$(Configuration)|\$(Platform)' == 'Debug|AnyCPU' ">
    <DebugType>full</DebugType>
    <Optimize>true</Optimize>
    <GenerateTailCalls>true</GenerateTailCalls>
  </PropertyGroup>

  <ItemGroup>
    <Compile Include="$SRC" />
${DEPS}
  </ItemGroup>
</Project>
EOL
# vim: set tw=0 ts=2 sw=2 expandtab:
