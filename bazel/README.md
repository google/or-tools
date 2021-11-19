# OR-Tools Bazel Build Instructions
| OS       | C++   |
|:-------- | :---: |
| Linux    | [![Status][linux_svg]][linux_link] |
| MacOS    | [![Status][macos_svg]][macos_link] |
| Windows  | [![Status][windows_svg]][windows_link] |

[linux_svg]: https://github.com/google/or-tools/actions/workflows/bazel_linux.yml/badge.svg?branch=master
[linux_link]: https://github.com/google/or-tools/actions/workflows/bazel_linux.yml
[macos_svg]: https://github.com/google/or-tools/actions/workflows/bazel_macos.yml/badge.svg?branch=master
[macos_link]: https://github.com/google/or-tools/actions/workflows/bazel_macos.yml
[windows_svg]: https://github.com/google/or-tools/actions/workflows/bazel_windows.yml/badge.svg?branch=master
[windows_link]: https://github.com/google/or-tools/actions/workflows/bazel_windows.yml

Dockers [Alpine, Archlinux, Centos, Debian, Fedora, OpenSuse, Ubuntu]: [![Status][docker_svg]][docker_link]

[docker_svg]: https://github.com/google/or-tools/actions/workflows/bazel_docker.yml/badge.svg?branch=master
[docker_link]: https://github.com/google/or-tools/actions/workflows/bazel_docker.yml

## Introduction

<nav for="bazel"> |
<a href="#dependencies">Dependencies</a> |
<a href="#compilation">Compilation</a> |
<a href="#testing">Testing</a> |
<a href="#integration">Integration</a> |
<a href="doc/ci.md">CI</a> |
</nav>

OR-Tools comes with a Bazel based build ([WORKSPACE](../WORKSPACE)) that can be
used on a wide range of platforms. If you don't have Bazel installed already,
you can download it for free from <https://bazel.build/>.

**warning: Currently OR-Tools Bazel doesn't support Python, Java nor .Net,
please use the Makefile or CMake based build instead.**

## Dependencies

OR-Tools depends on severals mandatory libraries.

* Google Abseil-cpp,
* Google Protobuf,
* Google Gtest,
* Bliss,
* SCIP,
* GLPK (GNU Linear Programming Kit)

## Compilation

You must compile OR-Tools using C++17:

* on UNIX:
  ```sh
  bazel build --cxxopt=-std=c++17 //...:all
  ```
* on Windows when using MSVC:
  ```sh
  bazel build --cxxopt="-std:c++17" //...:all
  ```

## Testing

You may run tests using:

* on UNIX:
  ```sh
  bazel test --cxxopt=-std=c++17 //...:all
  ```
* on Windows when using MSVC:
  ```sh
  bazel test --cxxopt="-std:c++17" //...:all
  ```

## Integration

To integrate OR-Tools in your own Bazel project,
you can take a look at the template project:
[or-tools/bazel\_or-tools](https://github.com/or-tools/bazel_or-tools).
