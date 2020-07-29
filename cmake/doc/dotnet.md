| Linux | macOS | Windows |
|-------|-------|---------|
| [![Status][dotnet_linux_svg]][dotnet_linux_link] | [![Status][dotnet_osx_svg]][dotnet_osx_link] | [![Status][dotnet_win_svg]][dotnet_win_link] |

[dotnet_linux_svg]: https://github.com/google/or-tools/workflows/.Net%20Linux%20CI/badge.svg
[dotnet_linux_link]: https://github.com/google/or-tools/actions?query=workflow%3A".Net+Linux+CI"
[dotnet_osx_svg]: https://github.com/google/or-tools/workflows/.Net%20MacOS%20CI/badge.svg
[dotnet_osx_link]: https://github.com/google/or-tools/actions?query=workflow%3A".Net+MacOS+CI"
[dotnet_win_svg]: https://github.com/google/or-tools/workflows/.Net%20Windows%20CI/badge.svg
[dotnet_win_link]: https://github.com/google/or-tools/actions?query=workflow%3A".Net+Windows+CI"

# Introduction
Try to build a .NetStandard2.0 native (for win-x64, linux-x64 and osx-x64) nuget multi package using [`dotnet/cli`](https://github.com/dotnet/cli) and the *new* .csproj format.

# Build the Binary Packages
To build the .Net nuget packages, simply run:
```sh
cmake -S. -Bbuild -DBUILD_DEPS=ON -DBUILD_DOTNET=ON
cmake --build build --target dotnet_package -v
```
note: Since `dotnet_package` is in target `all`, you can also ommit the
`--target` option.

# Technical Notes
First you should take a look at my [dotnet-native](https://github.com/Mizux/dotnet-native) project to understand the layout.  
Here I will only focus on the CMake/SWIG tips and tricks.

## Build directory layout
Since .Net use a `.csproj` project file to orchestrate everything we need to
generate them and have the following layout:

```shell
<CMAKE_BINARY_DIR>/dotnet
── Directory.Build.props
├── orLogo.png
├── ortools
│   ├── algorithms
│   │   ├── *.cs
│   │   ├── knapsack_solverCSHARP_wrap.cxx
│   │   ├── operations_research_algorithms.cs
│   │   └── operations_research_algorithmsPINVOKE.cs
│   ├── constraint_solver
│   │   ├── *.cs
│   │   ├── operations_research_constraint_solver.cs
│   │   ├── operations_research_constraint_solverPINVOKE.cs
│   │   ├── routingCSHARP_wrap.cxx
│   │   └── routingCSHARP_wrap.h
│   ├── graph
│   │   ├── *.cs
│   │   ├── graphCSHARP_wrap.cxx
│   │   ├── operations_research_graph.cs
│   │   └── operations_research_graphPINVOKE.cs
│   ├── linear_solver
│   │   ├── *.cs
│   │   ├── LinearSolver.pb.cs
│   │   ├── operations_research_linear_solver.cs
│   │   └── operations_research_linear_solverPINVOKE.cs
│   ├── sat
│   │   ├── *.cs
│   │   ├── BooleanProblem.pb.cs
│   │   ├── CpModel.pb.cs
│   │   ├── SatParameters.pb.cs
│   │   ├── operations_research_sat.cs
│   │   ├── operations_research_satPINVOKE.cs
│   │   ├── satCSHARP_wrap.cxx
│   │   └── satCSHARP_wrap.h
│   └── util
│       ├── *.cs
│       ├── operations_research_utilPINVOKE.cs
│       ├── sorted_interval_listCSHARP_wrap.cxx
│       └── sorted_interval_listCSHARP_wrap.h
├── Google.OrTools.runtime.linux-x64
│   └── Google.OrTools.runtime.linux-x64.csproj
├── Google.OrTools
│   └── Google.OrTools.csproj
├── replace.cmake
└── replace_runtime.cmake
```
src: `tree build/dotnet --prune -I "obj|bin"`

## Table of Content
* [Requirement](#requirement)
* [Directory Layout](#directory-layout)
* [Build Process](#build-process)
  * [Local Google.OrTools Package](#local-googleortools-package)
    * [Building a runtime Google.OrTools Package](#building-local-runtime-googleortools-package)
    * [Building a Local Google.OrTools Package](#building-local-googleortools-package)
    * [Testing the Local Google.OrTools Package](#testing-local-googleortools-package)
  * [Complete Google.OrTools Package](#complete-googleortools-package)
    * [Building all runtime Google.OrTools Package](#building-all-runtime-googleortools-package)
    * [Building a Complete Google.OrTools Package](#building-complete-googleortools-package)
    * [Testing the Complete Google.OrTools Package](#testing-complete-googleortools-package)
* [Appendices](#appendices)
  * [Ressources](#ressources)
  * [Issues](#issues)
* [Misc](#misc)

# Requirement
You'll need the ".Net Core SDK 3.1" to get the dotnet cli.
i.e. We won't/can't rely on VS 2019 since we want a portable cross-platform [`dotnet/cli`](https://github.com/dotnet/cli) pipeline.

# Directory Layout
* [`src/dotnet/Google.OrTools.runtime.linux-x64`](src/dotnet/Google.OrTools.runtime.linux-x64)
Contains the hypothetical C++ linux-x64 shared library.
* [`src/dotnet/Google.OrTools.runtime.osx-x64`](src/dotnet/Google.OrTools.runtime.osx-x64)
Contains the hypothetical C++ osx-x64 shared library.
* [`src/dotnet/Google.OrTools.runtime.win-x64`](src/dotnet/Google.OrTools.runtime.win-x64)
Contains the hypothetical C++ win-x64 shared library.
* [`src/dotnet/Google.OrTools`](src/dotnet/Google.OrTools)
Is the .NetStandard2.0 library which should depends on all previous available packages.
* [`src/dotnet/Google.OrToolsApp`](src/dotnet/Google.OrToolsApp)
Is a .NetCoreApp2.1 application with a **`PackageReference`** to `Google.OrTools` project to test.

note: While Microsoft use `runtime-<rid>.Company.Project` for native libraries naming,
it is very difficult to get ownership on it, so you should prefer to use`Company.Project.runtime-<rid>` instead since you can have ownership on `Company.*` prefix more easily.

# Build Process
To Create a native dependent package we will split it in two parts:
* A bunch of `Google.OrTools.runtime.{rid}.nupkg` packages for each
[Runtime Identifier (RId)](https://docs.microsoft.com/en-us/dotnet/core/rid-catalog) targeted.
* A meta-package `Google.OrTools.nupkg` depending on each runtime packages.

note: [`Microsoft.NetCore.App` packages](https://www.nuget.org/packages?q=Microsoft.NETCore.App)
follow this layout.

We have two use case scenario:
1. Locally, be able to build a Google.OrTools package which **only** target the local `OS Platform`,
i.e. building for only one 
[Runtime Identifier (RID)](https://docs.microsoft.com/en-us/dotnet/core/rid-catalog).  
note: This is useful when the C++ build is a complex process for Windows, Linux and MacOS.  
i.e. You don't support cross-compilation for the native library.

2. Be able to create a complete cross-platform (ed. platform as multiple rid) Google.OrTools package.  
i.e. First you generate each native Nuget package (`runtime.{rid}.Google.OrTools.nupkg`) 
on each native architecture, then copy paste these artifacts on one native machine
to generate the meta-package `Google.OrTools`.

## Local Google.OrTools Package
Let's start with scenario 1: Create a *Local* `Google.OrTools.nupkg` package targeting **one** [Runtime Identifier (RID)](https://docs.microsoft.com/en-us/dotnet/core/rid-catalog).  
We would like to build a `Google.OrTools.nupkg` package which only depends on one `Google.OrTools.runtime.{rid}.nupkg` in order to dev/test locally.  

The pipeline for `linux-x64` should be as follow:  
note: The pipeline will be similar for `osx-x64` and `win-x64` architecture, don't hesitate to look at the CI log.
![Local Pipeline](doc/local_pipeline.svg)
![Legend](doc/legend.svg)

### Building local Google.OrTools.runtime Package
disclaimer: for simplicity, in this git repository, we suppose the `g++` and `swig` process has been already performed.  
Thus we have the C++ shared library `Native.so` and the swig generated C# wrapper `Native.cs` already available.  
note: For a C++ CMake cross-platform project sample, take a look at [Mizux/cmake-cpp](https://github.com/Mizux/cmake-cpp).   
note: For a C++/Swig CMake cross-platform project sample, take a look at [Mizux/cmake-swig](https://github.com/Mizux/cmake-swig).

So first let's create the local `Google.OrTools.runtime.{rid}.nupkg` nuget package.

Here some dev-note concerning this `Google.OrTools.runtime.{rid}.csproj`.
* `AssemblyName` must be `Google.OrTools.dll` i.e. all {rid} projects **must** generate an assembly with the **same** name (i.e. no {rid} in the name)
  ```xml
  <RuntimeIdentifier>{rid}</RuntimeIdentifier>
  <AssemblyName>Google.OrTools</AssemblyName>
  <PackageId>Google.OrTools.runtime.{rid}</PackageId>
  ```
* Once you specify a `RuntimeIdentifier` then `dotnet build` or `dotnet build -r {rid}`
will behave identically (save you from typing it).
  - note: not the case if you use `RuntimeIdentifiers` (notice the 's')
* It is [recommended](https://docs.microsoft.com/en-us/nuget/create-packages/native-packages)
to add the tag `native` to the
[nuget package tags](https://docs.microsoft.com/en-us/dotnet/core/tools/csproj#packagetags)
  ```xml
  <PackageTags>native</PackageTags>
  ```
* Specify the output target folder for having the assembly output in `runtimes/{rid}/lib/{framework}` in the nupkg
  ```xml
  <BuildOutputTargetFolder>runtimes/$(RuntimeIdentifier)/lib</BuildOutputTargetFolder>
  ```
  note: Every files with an extension different from `.dll` will be filter out by nuget.
* Add the native shared library to the nuget package in the repository `runtimes/{rib}/native`. e.g. for linux-x64:
  ```xml
  <Content Include="*.so">
    <PackagePath>runtimes/linux-x64/native/%(Filename)%(Extension)</PackagePath>
    <Pack>true</Pack>
    <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>
  </Content>
  ```
* Generate the runtime package to a defined directory (i.e. so later in meta OrTools package we will be able to locate it)
  ```xml
  <PackageOutputPath>{...}/packages</PackageOutputPath>
  ```
* Generate the Reference Assembly (but don't include it to this runtime nupkg !, see below for explanation) using:
  ```xml
  <ProduceReferenceAssembly>true</ProduceReferenceAssembly>
  ```

Then you can generate the package using:
```bash
dotnet pack src/runtime.{rid}.OrTools
```
note: this will automatically trigger the `dotnet build`.

If everything good the package (located where your `PackageOutputPath` was defined) should have this layout:
```
{...}/packages/Google.OrTools.runtime.{rid}.nupkg:
\- Google.OrTools.runtime.{rid}.nuspec
\- runtimes
   \- {rid}
      \- lib
         \- {framework}
            \- Google.OrTools.dll
      \- native
         \- *.so / *.dylib / *.dll
...
```
note: `{rid}` could be `linux-x64` and `{framework}` could be `netstandard2.0`

tips: since nuget package are zip archive you can use `unzip -l <package>.nupkg` to study their layout.

### Building local Google.OrTools Package
So now, let's create the local `Google.OrTools.nupkg` nuget package which will depend on our previous runtime package.

Here some dev-note concerning this `OrTools.csproj`.
* This package is a meta-package so we don't want to ship an empty assembly file:
  ```xml
  <IncludeBuildOutput>false</IncludeBuildOutput>
  ```
* Add the previous package directory:
  ```xml
  <RestoreSources>{...}/packages;$(RestoreSources)</RestoreSources>
  ```
* Add dependency (i.e. `PackageReference`) on each runtime package(s) availabe:
  ```xml
  <ItemGroup Condition="Exists('{...}/packages/Google.OrTools.runtime.linux-x64.1.0.0.nupkg')">
    <PackageReference Include="Google.OrTools.runtime.linux-x64" Version="1.0.0" />
  </ItemGroup>
  ```
  Thanks to the `RestoreSource` we can work locally we our just builded package
  without the need to upload it on [nuget.org](https://www.nuget.org/).
* To expose the .Net Surface API the `OrTools.csproj` must contains a least one
[Reference Assembly](https://docs.microsoft.com/en-us/nuget/reference/nuspec#explicit-assembly-references) of the previously rumtime package.
  ```xml
  <Content Include="../Google.OrTools.runtime.{rid}/bin/$(Configuration)/$(TargetFramework)/{rid}/ref/*.dll">
    <PackagePath>ref/$(TargetFramework)/%(Filename)%(Extension)</PackagePath>
    <Pack>true</Pack>
    <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>
  </Content>
  ```

Then you can generate the package using:
```bash
dotnet pack src/.OrTools
```

If everything good the package (located where your `PackageOutputPath` was defined) should have this layout:
```
{...}/packages/Google.OrTools.nupkg:
\- Google.OrTools.nuspec
\- ref
   \- {framework}
      \- Google.OrTools.dll
...
```
note: `{framework}` could be `netstandard2.0`

### Testing local Google.OrTools Package
We can test everything is working by using the `OrToolsApp` project.

First you can build it using:
```
dotnet build src/OrToolsApp
```
note: Since OrToolsApp `PackageReference` OrTools and add `{...}/packages` to the `RestoreSource`.
During the build of OrToolsApp you can see that `Google.OrTools` and
`Google.OrTools.runtime.{rid}` are automatically installed in the nuget cache.

Then you can run it using:
```
dotnet build src/OrToolsApp
```

You should see something like this
```bash
[1] Enter OrToolsApp
[2] Enter OrTools
[3] Enter OrTools.{rid}
[3] Exit OrTools.{rid}
[2] Exit OrTools
[1] Exit OrToolsApp
```

## Complete Google.OrTools Package
Let's start with scenario 2: Create a *Complete* `Google.OrTools.nupkg` package targeting multiple [Runtime Identifier (RID)](https://docs.microsoft.com/en-us/dotnet/core/rid-catalog).  
We would like to build a `Google.OrTools.nupkg` package which depends on several `Google.OrTools.runtime.{rid}.nupkg`.  

The pipeline should be as follow:  
note: This pipeline should be run on any architecture,
provided you have generated the three architecture dependent `Google.OrTools.runtime.{rid}.nupkg` nuget packages.
![Full Pipeline](doc/full_pipeline.svg)
![Legend](doc/legend.svg)

### Building All runtime Google.OrTools Package
Like in the previous scenario, on each targeted OS Platform you can build the corresponding
`runtime.{rid}.Google.OrTools.nupkg` package.

Simply run on each platform
```bash
dotnet build src/runtime.{rid}.OrTools
dotnet pack src/runtime.{rid}.OrTools
```
note: replace `{rid}` by the Runtime Identifier associated to the current OS platform.

Then on one machine used, you copy all other packages in the `{...}/packages` so
when building `OrTools.csproj` we can have access to all package...

### Building Complete Google.OrTools Package
This is the same step than in the previous scenario, since we "see" all runtime
packages in `{...}/packages`, the project will depends on each of them.

Once copied all runtime package locally, simply run:
```bash
dotnet build src/OrTools
dotnet pack src/OrTools
```

### Testing Complete Google.OrTools Package
We can test everything is working by using the `OrToolsApp` project.

First you can build it using:
```
dotnet build src/OrToolsApp
```
note: Since OrToolsApp `PackageReference` OrTools and add `{...}/packages` to the `RestoreSource`.
During the build of OrToolsApp you can see that `Google.OrTools` and
`runtime.{rid}.Google.OrTools` are automatically installed in the nuget cache.

Then you can run it using:
```
dotnet run --project src/OrToolsApp
```

You should see something like this
```bash
[1] Enter OrToolsApp
[2] Enter OrTools
[3] Enter OrTools.{rid}
[3] Exit OrTools.{rid}
[2] Exit OrTools
[1] Exit OrToolsApp
```

# Ressources
Few links on the subject...

## Documention
First take a look at my [dotnet-native](https://github.com/Mizux/dotnet-native) project.

## Related Issues
Some issue related to this process
* [Nuget needs to support dependencies specific to target runtime #1660](https://github.com/NuGet/Home/issues/1660)
* [Improve documentation on creating native packages #238](https://github.com/NuGet/docs.microsoft.com-nuget/issues/238)

## Runtime IDentifier (RID)
* [.NET Core RID Catalog](https://docs.microsoft.com/en-us/dotnet/core/rid-catalog)
* [Creating native packages](https://docs.microsoft.com/en-us/nuget/create-packages/native-packages)
* [Blog on Nuget Rid Graph](https://natemcmaster.com/blog/2016/05/19/nuget3-rid-graph/)

## Reference on .csproj format
* [Common MSBuild project properties](https://docs.microsoft.com/en-us/visualstudio/msbuild/common-msbuild-project-properties?view=vs-2017)
* [MSBuild well-known item metadata](https://docs.microsoft.com/en-us/visualstudio/msbuild/msbuild-well-known-item-metadata?view=vs-2017)
* [Additions to the csproj format for .NET Core](https://docs.microsoft.com/en-us/dotnet/core/tools/csproj)
