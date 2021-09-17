# OR-Tools Makefile Build Instructions
| OS       | C++   | Python   | Java   | .NET   |
|:-------- | :---: | :------: | :----: | :----: |
| Linux    | [![Status][linux_cpp_svg]][linux_cpp_link] | [![Status][linux_python_svg]][linux_python_link] | [![Status][linux_java_svg]][linux_java_link] | [![Status][linux_dotnet_svg]][linux_dotnet_link] |
| MacOS    | [![Status][macos_cpp_svg]][macos_cpp_link] | [![Status][macos_python_svg]][macos_python_link] | [![Status][macos_java_svg]][macos_java_link] | [![Status][macos_dotnet_svg]][macos_dotnet_link] |
| Windows  | [![Status][windows_cpp_svg]][windows_cpp_link] | [![Status][windows_python_svg]][windows_python_link] | [![Status][windows_java_svg]][windows_java_link] | [![Status][windows_dotnet_svg]][windows_dotnet_link] |

[linux_cpp_svg]: https://github.com/google/or-tools/actions/workflows/make_linux_cpp.yml/badge.svg?branch=master
[linux_cpp_link]: https://github.com/google/or-tools/actions/workflows/make_linux_cpp.yml
[linux_python_svg]: https://github.com/google/or-tools/actions/workflows/make_linux_python.yml/badge.svg?branch=master
[linux_python_link]: https://github.com/google/or-tools/actions/workflows/make_linux_python.yml
[linux_java_svg]: https://github.com/google/or-tools/actions/workflows/make_linux_java.yml/badge.svg?branch=master
[linux_java_link]: https://github.com/google/or-tools/actions/workflows/make_linux_java.yml
[linux_dotnet_svg]: https://github.com/google/or-tools/actions/workflows/make_linux_dotnet.yml/badge.svg?branch=master
[linux_dotnet_link]: https://github.com/google/or-tools/actions/workflows/make_linux_dotnet.yml

[macos_cpp_svg]: https://github.com/google/or-tools/actions/workflows/make_macos_cpp.yml/badge.svg?branch=master
[macos_cpp_link]: https://github.com/google/or-tools/actions/workflows/make_macos_cpp.yml
[macos_python_svg]: https://github.com/google/or-tools/actions/workflows/make_macos_python.yml/badge.svg?branch=master
[macos_python_link]: https://github.com/google/or-tools/actions/workflows/make_macos_python.yml
[macos_java_svg]: https://github.com/google/or-tools/actions/workflows/make_macos_java.yml/badge.svg?branch=master
[macos_java_link]: https://github.com/google/or-tools/actions/workflows/make_macos_java.yml
[macos_dotnet_svg]: https://github.com/google/or-tools/actions/workflows/make_macos_dotnet.yml/badge.svg?branch=master
[macos_dotnet_link]: https://github.com/google/or-tools/actions/workflows/make_macos_dotnet.yml

[windows_cpp_svg]: https://github.com/google/or-tools/actions/workflows/make_windows_cpp.yml/badge.svg?branch=master
[windows_cpp_link]: https://github.com/google/or-tools/actions/workflows/make_windows_cpp.yml
[windows_python_svg]: https://github.com/google/or-tools/actions/workflows/make_windows_python.yml/badge.svg?branch=master
[windows_python_link]: https://github.com/google/or-tools/actions/workflows/make_windows_python.yml
[windows_java_svg]: https://github.com/google/or-tools/actions/workflows/make_windows_java.yml/badge.svg?branch=master
[windows_java_link]: https://github.com/google/or-tools/actions/workflows/make_windows_java.yml
[windows_dotnet_svg]: https://github.com/google/or-tools/actions/workflows/make_windows_dotnet.yml/badge.svg?branch=master
[windows_dotnet_link]: https://github.com/google/or-tools/actions/workflows/make_windows_dotnet.yml

Dockers [Alpine, Archlinux, Centos, Debian, Fedora, OpenSuse, Ubuntu]x[C++,
Python, Java, .Net]: [![Status][docker_svg]][docker_link]

[docker_svg]: https://github.com/google/or-tools/actions/workflows/make_docker.yml/badge.svg?branch=master
[docker_link]: https://github.com/google/or-tools/actions/workflows/make_docker.yml

## Introduction
<nav for="make"> |
<a href="#deps">Dependencies</a> |
<a href="doc/ci.md">CI</a> |
</nav>

OR-Tools comes with a GNU Make based build ([Makefile](../Makefile)) that can be
used on a wide range of platforms.

## [Dependencies](#deps)

OR-Tools depends on severals mandatory libraries. You can compile them all using
the target `third_party` or you can compile few of them and give the
installation directory to the others using the Make variable below.

* ZLIB (`UNIX_ZLIB_DIR` or `WINDOWS_ZLIB_DIR`),
* Google Abseil-cpp (`UNIX_ABSL_DIR` or `WINDOWS_ABSL_DIR`),
* Google Protobuf (`UNIX_PROTOBUF_DIR` or `WINDOWS_PROTOBUF_DIR`),
* SCIP (`UNIX_SCIP_DIR` or `WINDOWS_SCIP_DIR`),
* COIN-OR CoinUtils (`UNIX_COINUTILS_DIR` or `WINDOWS_COINUTILS_DIR`),
* COIN-OR Osi (`UNIX_OSI_DIR` or `WINDOWS_OSI_DIR`),
* COIN-OR Clp (`UNIX_CLP_DIR` or `WINDOWS_CLP_DIR`),
* COIN-OR Cgl (`UNIX_CGL_DIR` or `WINDOWS_CGL_DIR`),
* COIN-OR Cbc (`UNIX_CBC_DIR` or `WINDOWS_CBC_DIR`),

OR-Tools also have few (ed compile time) optional solvers support (disabled by
default):

* CPLEX (`UNIX_CPLEX_DIR` or `WINDOWS_CPLEX_DIR`),
* GLPK (`UNIX_GLPK_DIR` or `WINDOWS_GLPK_DIR`),
* GUROBI (`UNIX_GUROBI_DIR` or `WINDOWS_GUROBI_DIR`),
* XPRESS (`UNIX_XPRESS_DIR` or `WINDOWS_XPRESS_DIR`)

**warning: Since these solvers require license and are proprietary, we can't
test it on public CI and support can be broken.**
