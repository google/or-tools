| Linux | macOS | Windows |
|-------|-------|---------|
| [![Status][linux_dotnet_svg]][linux_dotnet_link] | [![Status][macos_dotnet_svg]][macos_dotnet_link] | [![Status][windows_dotnet_svg]][windows_dotnet_link] |

[linux_dotnet_svg]: https://github.com/google/or-tools/actions/workflows/cmake_linux_dotnet.yml/badge.svg?branch=master
[linux_dotnet_link]: https://github.com/google/or-tools/actions/workflows/cmake_linux_dotnet.yml
[macos_dotnet_svg]: https://github.com/google/or-tools/actions/workflows/cmake_macos_dotnet.yml/badge.svg?branch=master
[macos_dotnet_link]: https://github.com/google/or-tools/actions/workflows/cmake_macos_dotnet.yml
[windows_dotnet_svg]: https://github.com/google/or-tools/actions/workflows/cmake_windows_dotnet.yml/badge.svg?branch=master
[windows_dotnet_link]: https://github.com/google/or-tools/actions/workflows/cmake_windows_dotnet.yml

# Introduction
Try to build a .NetStandard2.0 native (for win-x64, linux-x64 and osx-x64) nuget multi package using [`dotnet/cli`](https://github.com/dotnet/cli) and the *new* .csproj format.

# Build the Binary Packages
To build the .Net nuget packages, simply run:
```sh
cmake -S. -Bbuild -DBUILD_DOTNET=ON
cmake --build build --target dotnet_package -v
```
note: Since `dotnet_package` is in target `all`, you can also ommit the
`--target` option.

# Testing
To list tests:
```sh
cd build
ctest -N
```

To only run .NET tests:
```sh
cd build
ctest -R "dotnet_.*"
```

# Technical Notes
First you should take a look at the [ortools/dotnet/README.md](../../ortools/dotnet/README.md) to understand the layout.  
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
