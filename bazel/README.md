# OR-Tools Bazel Build Instructions
[![Status][docker_svg]][docker_link]

[docker_svg]: https://github.com/google/or-tools/workflows/Docker%20Bazel/badge.svg
[docker_link]: https://github.com/google/or-tools/actions?query=workflow%3A"Docker+Bazel"

## Introduction
<nav for="bazel"> |
<a href="#deps">Dependencies</a> |
<a href="#integration">Integration</a> |
<a href="doc/ci.md">CI</a> |
</nav>

OR-Tools comes with a Bazel based build ([WORKSPACE](../WORKSPACE)) that can be
used on a wide range of platforms. If you don't have Bazel installed already,
you can download it for free from <https://bazel.build/>.

**warning: Currently OR-Tools Bazel doesn't support Python, Java nor .Net, please use
the Makefile or CMake based build instead.**

## [Dependencies](#deps)
OR-Tools depends on severals mandatory libraries.

* Google Abseil-cpp,
* Google Gflags,
* Google Glog,
* Google Protobuf,
* Google Gtest,
* GLPK (GNU Linear Programming Kit)

## Compilation
You must compile OR-Tools using C++17:
* on UNIX: `--cxxopt=-std=c++17`
* on Windows when using MSVC: `--cxxopt="-std:c++17"`

## [Integrating OR-Tools in your Bazel Project](#integration)
TODO
