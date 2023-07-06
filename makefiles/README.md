# OR-Tools Makefile Build Instructions
| OS       | C++   | Python   | Java   | .NET   |
|:-------- | :---: | :------: | :----: | :----: |
| Linux    | [![Status][linux_cpp_svg]][linux_cpp_link] | [![Status][linux_python_svg]][linux_python_link] | [![Status][linux_java_svg]][linux_java_link] | [![Status][linux_dotnet_svg]][linux_dotnet_link] |
| MacOS    | [![Status][macos_cpp_svg]][macos_cpp_link] | [![Status][macos_python_svg]][macos_python_link] | [![Status][macos_java_svg]][macos_java_link] | [![Status][macos_dotnet_svg]][macos_dotnet_link] |
| Windows  | [![Status][windows_cpp_svg]][windows_cpp_link] | [![Status][windows_python_svg]][windows_python_link] | [![Status][windows_java_svg]][windows_java_link] | [![Status][windows_dotnet_svg]][windows_dotnet_link] |

[linux_cpp_svg]: ./../../../actions/workflows/amd64_linux_make_cpp.yml/badge.svg?branch=main
[linux_cpp_link]: ./../../../actions/workflows/amd64_linux_make_cpp.yml
[linux_python_svg]: ./../../../actions/workflows/amd64_linux_make_python.yml/badge.svg?branch=main
[linux_python_link]: ./../../../actions/workflows/amd64_linux_make_python.yml
[linux_java_svg]: ./../../../actions/workflows/amd64_linux_make_java.yml/badge.svg?branch=main
[linux_java_link]: ./../../../actions/workflows/amd64_linux_make_java.yml
[linux_dotnet_svg]: ./../../../actions/workflows/amd64_linux_make_dotnet.yml/badge.svg?branch=main
[linux_dotnet_link]: ./../../../actions/workflows/amd64_linux_make_dotnet.yml

[macos_cpp_svg]: ./../../../actions/workflows/amd64_macos_make_cpp.yml/badge.svg?branch=main
[macos_cpp_link]: ./../../../actions/workflows/amd64_macos_make_cpp.yml
[macos_python_svg]: ./../../../actions/workflows/amd64_macos_make_python.yml/badge.svg?branch=main
[macos_python_link]: ./../../../actions/workflows/amd64_macos_make_python.yml
[macos_java_svg]: ./../../../actions/workflows/amd64_macos_make_java.yml/badge.svg?branch=main
[macos_java_link]: ./../../../actions/workflows/amd64_macos_make_java.yml
[macos_dotnet_svg]: ./../../../actions/workflows/amd64_macos_make_dotnet.yml/badge.svg?branch=main
[macos_dotnet_link]: ./../../../actions/workflows/amd64_macos_make_dotnet.yml

[windows_cpp_svg]: ./../../../actions/workflows/amd64_windows_make_cpp.yml/badge.svg?branch=main
[windows_cpp_link]: ./../../../actions/workflows/amd64_windows_make_cpp.yml
[windows_python_svg]: ./../../../actions/workflows/amd64_windows_make_python.yml/badge.svg?branch=main
[windows_python_link]: ./../../../actions/workflows/amd64_windows_make_python.yml
[windows_java_svg]: ./../../../actions/workflows/amd64_windows_make_java.yml/badge.svg?branch=main
[windows_java_link]: ./../../../actions/workflows/amd64_windows_make_java.yml
[windows_dotnet_svg]: ./../../../actions/workflows/amd64_windows_make_dotnet.yml/badge.svg?branch=main
[windows_dotnet_link]: ./../../../actions/workflows/amd64_windows_make_dotnet.yml

Dockers [Alpine, Archlinux, Centos, Debian, Fedora, OpenSuse, Ubuntu]x[C++,
Python, Java, .Net]: [![Status][docker_svg]][docker_link]

[docker_svg]: ./../../../actions/workflows/amd64_docker_make.yml/badge.svg?branch=main
[docker_link]: ./../../../actions/workflows/amd64_docker_make.yml

## Introduction
<nav for="make"> |
<a href="#requirement">Requirement</a> |
<a href="#dependencies">Dependencies</a> |
<a href="docs/ci.md">CI</a> |
</nav>

OR-Tools comes with a GNU Make based build ([Makefile](../Makefile)) that can be
used on a wide range of platforms.

## Requirement
You'll need:

* `GNU Make >= 4.3`.
* A C++20 compiler (gcc 10 or above)

## Dependencies

OR-Tools depends on several mandatory libraries, either as infrastructure or as
optimization solvers. You can either compile all of these dependencies using the
target `third_party` or compile some of them on your own and give their
installation directories to the others using the Make variables below.

* zlib (`UNIX_ZLIB_DIR` or `WINDOWS_ZLIB_DIR`),
* Google Abseil-cpp (`UNIX_ABSL_DIR` or `WINDOWS_ABSL_DIR`),
* Google Protobuf (`UNIX_PROTOBUF_DIR` or `WINDOWS_PROTOBUF_DIR`),
* SCIP (`UNIX_SCIP_DIR` or `WINDOWS_SCIP_DIR`),
* COIN-OR solvers:
  * COIN-OR CoinUtils (`UNIX_COINUTILS_DIR` or `WINDOWS_COINUTILS_DIR`),
  * COIN-OR Osi (`UNIX_OSI_DIR` or `WINDOWS_OSI_DIR`),
  * COIN-OR Clp (`UNIX_CLP_DIR` or `WINDOWS_CLP_DIR`),
  * COIN-OR Cgl (`UNIX_CGL_DIR` or `WINDOWS_CGL_DIR`),
  * COIN-OR Cbc (`UNIX_CBC_DIR` or `WINDOWS_CBC_DIR`),

OR-Tools can also optionally (disabled by default) be compiled with support for
the following third-party solvers:

* CPLEX (`UNIX_CPLEX_DIR` or `WINDOWS_CPLEX_DIR`),
* GLPK (`UNIX_GLPK_DIR` or `WINDOWS_GLPK_DIR`),
* GUROBI (`UNIX_GUROBI_DIR` or `WINDOWS_GUROBI_DIR`),
* XPRESS (`UNIX_XPRESS_DIR` or `WINDOWS_XPRESS_DIR`)

**warning: Since these solvers are either proprietary (and require a specific
license) or available under the GPL, we can't test them on public CI and their
support may be broken.**
