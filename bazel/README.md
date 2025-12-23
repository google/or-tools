# OR-Tools Bazel Build Instructions
| OS   | C++   |
|:---- | :---: |
| amd64 Linux   | [![Status][linux_svg]][linux_link] |
| arm64 MacOS   | [![Status][arm64_macos_svg]][arm64_macos_link] |
| amd64 MacOS   | [![Status][amd64_macos_svg]][amd64_macos_link] |
| amd64 Windows | [![Status][windows_svg]][windows_link] |

[linux_svg]: ./../../../actions/workflows/amd64_linux_bazel.yml/badge.svg?branch=main
[linux_link]: ./../../../actions/workflows/amd64_linux_bazel.yml

[arm64_macos_svg]: ./../../../actions/workflows/arm64_macos_bazel.yml/badge.svg?branch=main
[arm64_macos_link]: ./../../../actions/workflows/arm64_macos_bazel.yml

[amd64_macos_svg]: ./../../../actions/workflows/amd64_macos_bazel.yml/badge.svg?branch=main
[amd64_macos_link]: ./../../../actions/workflows/amd64_macos_bazel.yml

[windows_svg]: ./../../../actions/workflows/amd64_windows_bazel.yml/badge.svg?branch=main
[windows_link]: ./../../../actions/workflows/amd64_windows_bazel.yml

Dockers [Alpine, Archlinux, Centos, Debian, Fedora, OpenSuse, Ubuntu]: [![Status][docker_svg]][docker_link]

[docker_svg]: ./../../../actions/workflows/amd64_docker_bazel.yml/badge.svg?branch=main
[docker_link]: ./../../../actions/workflows/amd64_docker_bazel.yml

## Introduction

<nav for="bazel"> |
<a href="#requirement">Requirement</a> |
<a href="#dependencies">Dependencies</a> |
<a href="#compilation">Compilation</a> |
<a href="#testing">Testing</a> |
<a href="#integration">Integration</a> |
<a href="docs/ci.md">CI</a> |
</nav>

OR-Tools comes with a Bazel based build ([WORKSPACE](../WORKSPACE)) that can be
used on a wide range of platforms. If you don't have Bazel installed already,
you can download it for free from <https://bazel.build/>.

**warning: Currently OR-Tools Bazel doesn't support Python, Java nor .Net,
please use the Makefile or CMake based build instead.**

## Requirement

You'll need:

* `Bazel >= 5.4.0`.
* A compiler with C++17 support.

## Solvers supported

Here the list of supported solvers:

*   CP-SAT
*   GLOP
*   GLPK
*   PDLP
*   SCIP

## Dependencies

OR-Tools depends on several mandatory libraries.

*   Eigen
*   Google Abseil-cpp,
*   Google Protobuf,
*   Google Gtest,
*   Bliss,
*   SCIP,
*   GLPK (GNU Linear Programming Kit)

## Compilation

```sh
bazel build -c opt //ortools/... //examples/...
```

## Testing

```sh
bazel test -c opt //ortools/... //examples/...
```

## Integration

To integrate OR-Tools in your own Bazel project,
you can take a look at the template project:
[or-tools/bazel\_or-tools](https://github.com/or-tools/bazel_or-tools).
