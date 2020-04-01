# OR-Tools Makefile Build Instructions

| OS      |                                    |
|:--------|------------------------------------|
| Linux   | [![Status][linux_svg]][linux_link] |
| MacOS   | [![Status][osx_svg]][osx_link]     |
| Windows | [![Status][win_svg]][win_link]     |

[linux_svg]: https://github.com/google/or-tools/workflows/Makefile/badge.svg
[linux_link]: https://github.com/google/or-tools/actions?query=workflow%3A"Makefile"

Dockers: [![Status][docker_svg]][docker_link]

[docker_svg]: https://github.com/google/or-tools/workflows/Docker%20Make/badge.svg
[docker_link]: https://github.com/google/or-tools/actions?query=workflow%3A"Docker+Make"

## Introduction
<nav for="make"> |
<a href="#deps">Dependencies</a> |
<a href="doc/ci.md">CI</a> |
</nav>

OR-Tools comes with a GNU Make based build ([Makefile](../Makefile)) that can be
used on a wide range of platforms.

## [Dependencies](#deps)
OR-Tools depends on severals mandatory libraries. You can compile them all
using the target `third_party` or you can compile few of them and give the
installation directory to the others using the Make variable below. 

* ZLIB (`UNIX_ZLIB_DIR` or `WINDOWS_ZLIB_DIR`),
* Google Abseil-cpp (`UNIX_ABSL_DIR` or `WINDOWS_ABSL_DIR`),
* Google Gflags (`UNIX_GFLAG_DIR` or `WINDOWS_GFLAG_DIR`),
* Google Glog (`UNIX_GLOG_DIR` or `WINDOWS_GLOG_DIR`),
* Google Protobuf (`UNIX_PROTOBUF_DIR` or `WINDOWS_PROTOBUF_DIR`),
* COIN-OR CoinUtils (`UNIX_COINUTILS_DIR` or `WINDOWS_COINUTILS_DIR`),
* COIN-OR Osi (`UNIX_OSI_DIR` or `WINDOWS_OSI_DIR`),
* COIN-OR Clp (`UNIX_CLP_DIR` or `WINDOWS_CLP_DIR`),
* COIN-OR Cgl (`UNIX_CGL_DIR` or `WINDOWS_CGL_DIR`),
* COIN-OR Cbc (`UNIX_CBC_DIR` or `WINDOWS_CBC_DIR`),

OR-Tools also have few (ed compile time) optional solvers support (disabled by
default):

*   SCIP (`UNIX_SCIP_DIR` or `WINDOWS_SCIP_DIR`),
*   CPLEX (`UNIX_CPLEX_DIR` or `WINDOWS_CPLEX_DIR`),
*   XPRESS (`UNIX_XPRESS_DIR` or `WINDOWS_XPRESS_DIR`)

**warning: Since these solvers require license and are proprietary, we can't
test it on public CI and support can be broken.**
