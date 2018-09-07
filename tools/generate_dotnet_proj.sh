#!/usr/bin/env bash
# usage: ./tools/generate_dotnet_csproj.sh

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
rm -rf examples/dotnet/bin examples/dotnet/obj
echo "Remove prevous .*proj .sln files..."
rm -f examples/dotnet/*.*proj
rm -f examples/dotnet/*.sln
echo "Remove prevous .*proj files...DONE"

##############
##  CSHARP  ##
##############
for FILE in examples/dotnet/*.cs; do
  # if no files found do nothing
  [ -e "$FILE" ] || continue
  PROJ="${FILE%.cs}.csproj";
  echo "Generate $PROJ..."
  BASE=$(basename "$FILE")
  cat >"$PROJ" <<EOL
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <OutputType>Exe</OutputType>
    <LangVersion>7.2</LangVersion>
    <TargetFramework>netcoreapp2.1</TargetFramework>
    <EnableDefaultCompileItems>false</EnableDefaultCompileItems>
    <RestoreSources>../../packages;\$(RestoreSources);https://api.nuget.org/v3/index.json</RestoreSources>
  </PropertyGroup>

  <PropertyGroup Condition=" '\$(Configuration)|\$(Platform)' == 'Debug|AnyCPU' ">
    <DebugType>full</DebugType>
    <Optimize>true</Optimize>
    <GenerateTailCalls>true</GenerateTailCalls>
  </PropertyGroup>

  <ItemGroup>
    <Compile Include="$BASE" />
    <PackageReference Include="Google.OrTools" Version="${OR_TOOLS_MAJOR}.${OR_TOOLS_MINOR}.*" />
  </ItemGroup>
</Project>
EOL
  echo "Generate $PROJ...DONE"
done

##############
##  CSHARP  ##
##############
for FILE in examples/dotnet/*.fs; do
  # if no files found do nothing
  [ -e "$FILE" ] || continue
  PROJ="${FILE%.fs}.fsproj";
  echo "Generate $PROJ..."
  BASE=$(basename "$FILE")
  cat >"$PROJ" <<EOL
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <OutputType>Exe</OutputType>
    <TargetFramework>netcoreapp2.1</TargetFramework>
    <EnableDefaultCompileItems>false</EnableDefaultCompileItems>
    <RestoreSources>../../packages;\$(RestoreSources);https://api.nuget.org/v3/index.json</RestoreSources>
  </PropertyGroup>

  <PropertyGroup Condition=" '\$(Configuration)|\$(Platform)' == 'Debug|AnyCPU' ">
    <DebugType>full</DebugType>
    <Optimize>true</Optimize>
    <GenerateTailCalls>true</GenerateTailCalls>
  </PropertyGroup>

  <ItemGroup>
    <Compile Include="$BASE" />
    <PackageReference Include="Google.OrTools.FSharp" Version="${OR_TOOLS_MAJOR}.${OR_TOOLS_MINOR}.*" />
  </ItemGroup>
</Project>
EOL
  echo "Generate $PROJ...DONE"
done

###############
##  Samples  ##
###############
for FILE in ortools/sat/samples/*.cs; do
  # if no files found do nothing
  [ -e "$FILE" ] || continue
  PROJ="${FILE%.cs}.csproj";
  echo "Generate $PROJ..."
  BASE=$(basename "$FILE")
  cat >"$PROJ" <<EOL
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <OutputType>Exe</OutputType>
    <LangVersion>7.2</LangVersion>
    <TargetFramework>netcoreapp2.1</TargetFramework>
    <EnableDefaultCompileItems>false</EnableDefaultCompileItems>
    <RestoreSources>../../../packages;\$(RestoreSources);https://api.nuget.org/v3/index.json</RestoreSources>
  </PropertyGroup>

  <PropertyGroup Condition=" '\$(Configuration)|\$(Platform)' == 'Debug|AnyCPU' ">
    <DebugType>full</DebugType>
    <Optimize>true</Optimize>
    <GenerateTailCalls>true</GenerateTailCalls>
  </PropertyGroup>

  <ItemGroup>
    <Compile Include="$BASE" />
    <PackageReference Include="Google.OrTools" Version="${OR_TOOLS_MAJOR}.${OR_TOOLS_MINOR}.*" />
  </ItemGroup>
</Project>
EOL
  echo "Generate $PROJ...DONE"
done

###########
##  SLN  ##
###########
SLN=Google.OrTools.Examples.sln
echo "Generate ${SLN}..."
cd examples/dotnet
dotnet new sln -n ${SLN%.sln}
for i in *.*proj; do
  dotnet sln ${SLN} add "$i"
done
echo "Generate ${SLN}...DONE"

# vim: set tw=0 ts=2 sw=2 expandtab:
