# OR-Tools CMake Build Instructions

| OS       | C++   | Python   | Java   | .NET   |
|:-------- | :---: | :------: | :----: | :----: |
| Linux    | [![Status][linux_cpp_svg]][linux_cpp_link] | [![Status][linux_python_svg]][linux_python_link] | [![Status][linux_java_svg]][linux_java_link] | [![Status][linux_dotnet_svg]][linux_dotnet_link] |
| MacOS    | [![Status][macos_cpp_svg]][macos_cpp_link] | [![Status][macos_python_svg]][macos_python_link] | [![Status][macos_java_svg]][macos_java_link] | [![Status][macos_dotnet_svg]][macos_dotnet_link] |
| Windows  | [![Status][windows_cpp_svg]][windows_cpp_link] | [![Status][windows_python_svg]][windows_python_link] | [![Status][windows_java_svg]][windows_java_link] | [![Status][windows_dotnet_svg]][windows_dotnet_link] |

[linux_cpp_svg]: ./../../../actions/workflows/amd64_linux_cmake_cpp.yml/badge.svg?branch=main
[linux_cpp_link]: ./../../../actions/workflows/amd64_linux_cmake_cpp.yml
[linux_python_svg]: ./../../../actions/workflows/amd64_linux_cmake_python.yml/badge.svg?branch=main
[linux_python_link]: ./../../../actions/workflows/amd64_linux_cmake_python.yml
[linux_java_svg]: ./../../../actions/workflows/amd64_linux_cmake_java.yml/badge.svg?branch=main
[linux_java_link]: ./../../../actions/workflows/amd64_linux_cmake_java.yml
[linux_dotnet_svg]: ./../../../actions/workflows/amd64_linux_cmake_dotnet.yml/badge.svg?branch=main
[linux_dotnet_link]: ./../../../actions/workflows/amd64_linux_cmake_dotnet.yml

[macos_cpp_svg]: ./../../../actions/workflows/amd64_macos_cmake_cpp.yml/badge.svg?branch=main
[macos_cpp_link]: ./../../../actions/workflows/amd64_macos_cmake_cpp.yml
[macos_python_svg]: ./../../../actions/workflows/amd64_macos_cmake_python.yml/badge.svg?branch=main
[macos_python_link]: ./../../../actions/workflows/amd64_macos_cmake_python.yml
[macos_java_svg]: ./../../../actions/workflows/amd64_macos_cmake_java.yml/badge.svg?branch=main
[macos_java_link]: ./../../../actions/workflows/amd64_macos_cmake_java.yml
[macos_dotnet_svg]: ./../../../actions/workflows/amd64_macos_cmake_dotnet.yml/badge.svg?branch=main
[macos_dotnet_link]: ./../../../actions/workflows/amd64_macos_cmake_dotnet.yml

[windows_cpp_svg]: ./../../../actions/workflows/amd64_windows_cmake_cpp.yml/badge.svg?branch=main
[windows_cpp_link]: ./../../../actions/workflows/amd64_windows_cmake_cpp.yml
[windows_python_svg]: ./../../../actions/workflows/amd64_windows_cmake_python.yml/badge.svg?branch=main
[windows_python_link]: ./../../../actions/workflows/amd64_windows_cmake_python.yml
[windows_java_svg]: ./../../../actions/workflows/amd64_windows_cmake_java.yml/badge.svg?branch=main
[windows_java_link]: ./../../../actions/workflows/amd64_windows_cmake_java.yml
[windows_dotnet_svg]: ./../../../actions/workflows/amd64_windows_cmake_dotnet.yml/badge.svg?branch=main
[windows_dotnet_link]: ./../../../actions/workflows/amd64_windows_cmake_dotnet.yml

Dockers \[Alpine, Archlinux, Centos, Debian, Fedora, OpenSuse, Ubuntu\]x
\[C++, Python, Java, .Net\]: [![Status][docker_svg]][docker_link]

[docker_svg]: ./../../../actions/workflows/amd64_docker_cmake.yml/badge.svg?branch=main
[docker_link]: ./../../../actions/workflows/amd64_docker_cmake.yml

[![Build Status][aarch64_toolchain_status]][aarch64_toolchain_link]
[![Build Status][mips_toolchain_status]][mips_toolchain_link]
[![Build Status][powerpc_toolchain_status]][powerpc_toolchain_link]

[aarch64_toolchain_status]: ./../../../actions/workflows/aarch64_toolchain.yml/badge.svg?branch=main
[aarch64_toolchain_link]: ./../../../actions/workflows/aarch64_toolchain.yml
[mips_toolchain_status]: ./../../../actions/workflows/mips_toolchain.yml/badge.svg?branch=main
[mips_toolchain_link]: ./../../../actions/workflows/mips_toolchain.yml
[powerpc_toolchain_status]: ./../../../actions/workflows/powerpc_toolchain.yml/badge.svg?branch=main
[powerpc_toolchain_link]: ./../../../actions/workflows/powerpc_toolchain.yml

## Introduction
<nav for="cmake"> |
<a href="#requirement">Requirement</a> |
<a href="#dependencies">Dependencies</a> |
<a href="#cmake-options">Options</a> |
<a href="docs/cpp.md">C++</a> |
<a href="docs/swig.md">Swig</a> |
<a href="docs/python.md">Python 3</a> |
<a href="docs/dotnet.md">.Net Core</a> |
<a href="docs/java.md">Java</a> |
<a href="#integrating-or-tools-in-your-cmake-project">Integration</a> |
<a href="docs/ci.md">CI</a> |
</nav>

OR-Tools comes with a CMake-based build ([CMakeLists.txt](../CMakeLists.txt))
that can be used on a wide range of platforms (the "C" stands for
cross-platform). If you don't have CMake installed already, you can download it
for free from <http://www.cmake.org/>.

CMake works by generating native Makefiles or build projects that can be used in
the compiler environment of your choice.<br>You can either build OR-Tools with
CMake as a standalone project or incorporate it into an existing CMake project.

## Requirement
You'll need:

* `CMake >= 3.18`.
* A C++20 compiler (GCC 10 or above)

## Solvers supported

Here the list of supported solvers:

* CBC
* CLP
* CP-SAT
* CPLEX\*
* GLOP
* GLPK\*
* HiGHS\*
* PDLP
* SCIP
* XPRESS

\*: these solvers are disabled by default.

## Dependencies

OR-Tools depends on several mandatory librariess, either as infrastructure or as
optimization solvers. You can either compile them all at configure time using
the option `-DBUILD_DEPS=ON` (`OFF` by default) or compile some of them using
the options below (see [CMake Options](#cmake-options) below).

*   zlib (`BUILD_ZLIB`),
*   Google Abseil-cpp (`BUILD_absl`),
*   Google Protobuf (`BUILD_Protobuf`),
*   COIN-OR solvers:
    *   COIN-OR CoinUtils (`BUILD_CoinUtils`),
    *   COIN-OR Osi (`BUILD_Osi`),
    *   COIN-OR Clp (`BUILD_Clp`),
    *   COIN-OR Cgl (`BUILD_Cgl`),
    *   COIN-OR Cbc (`BUILD_Cbc`),<br>
        note: You can disable the support of COIN-OR solvers
        (i.e. Cbc and Clp solver) by using `-DUSE_COINOR=OFF` (`ON` by default).
*   HIGHS (`BUILD_HIGHS`),<br>
    note: You must enable the support of HiGHS solver by using `-DUSE_HIGHS=ON`
    (`OFF` by default).
*   SCIP (`BUILD_SCIP`),<br>
    note: You can disable the support of SCIP solver by using `-DUSE_SCIP=OFF`
    (`ON` by default).

OR-Tools can also optionally (disabled by default i.e. `OFF`) be compiled with
support for the following third-party solvers:

*   GLPK (`BUILD_GLPK`),<br>
    note: You must enable the support of GLPK solver by using `-DUSE_GLPK=ON`
    (`OFF` by default).
*   CPLEX (`USE_CPLEX`),

**warning: Since these solvers are either proprietary (and require a specific
license) or available under the GPL, we can't test them on public CI and their
support may be broken.**

### Enabling CPLEX Support

To enable CPLEX support, configure with `-DUSE_CPLEX=ON` and
`-DCPLEX_ROOT=/absolute/path/to/CPLEX/root/dir`, replacing
`/absolute/path/to/CPLEX/root/dir` with the path to your CPLEX installation.
`CPLEX_ROOT` can also be defined as an environment variable rather than an
option at configure time.

For ease of migration from legacy `make third_party` builds, CMake will also
read the CPLEX installation path from the `UNIX_CPLEX_DIR` environment variable,
if defined.

## CMake Options

There are several options that can be passed to CMake to modify how the code
is built.<br>
To set these options and parameters, use `-D<Parameter_name>=<value>`.

All CMake options are passed at configure time, i.e., by running <br>
`cmake -S. -B<your_chosen_build_directory> -DOPTION_ONE=ON -DOPTION_TWO=OFF ...` <br>
before running `cmake --build <your_chosen_build_directory>`<br>

For example, to generate build files including dependencies in a new
subdirectory called 'build', run:

```sh
cmake -S. -Bbuild -DBUILD_DEPS:BOOL=ON
```
and then build with:

```sh
cmake --build build
```

Following is a list of available options, for the full list run:

```sh
cmake -S. -Bbuild -LH
```

| CMake Option | Default Value | Note |
|:-------------|:--------------|:-----|
| `CMAKE_BUILD_TYPE` | Release | see CMake documentation [here](https://cmake.org/cmake/help/latest/variable/CMAKE_BUILD_TYPE.html) |
| `BUILD_CXX` | ON | Build C++ |
| `BUILD_DOTNET` | OFF | Build .Net wrapper and packages |
| `BUILD_JAVA` | OFF | Build Java wrapper and packages |
| `BUILD_PYTHON` | OFF | Build Python wrapper and package |
| | | |
| `BUILD_FLATZINC` | ON\* | Build the flatzinc library<br>**Forced** to OFF if `BUILD_CXX=OFF` |
| `BUILD_GLOP` | OFF\* | Build the standalone Glop library<br>**Forced** to OFF if `BUILD_CXX=ON`, otherwise default to ON |
| | | |
| `BUILD_DEPS` | OFF* | Default to ON if `BUILD_JAVA=ON` or `BUILD_PYTHON=ON` or `BUILD_DOTNET=ON` |
| `BUILD_ZLIB` | OFF* | Static build the zlib library<br>**Forced** to ON if `BUILD_DEPS=ON` |
| `BUILD_absl` | OFF* | Static build the abseil-cpp libraries<br>**Forced** to ON if `BUILD_DEPS=ON` |
| `BUILD_Protobuf` | OFF* | Static build the protobuf libraries<br>**Forced** to ON if `BUILD_DEPS=ON` |
| `BUILD_re2`  | OFF* | Static build the re2 libraries<br>**Forced** to ON if `BUILD_DEPS=ON` |
| `BUILD_Eigen3` | OFF* | Static build the Eigen3 libraries<br>**Forced** to ON if `BUILD_DEPS=ON` |
| | | |
| `USE_COINOR` | ON\* | Enable Coin-OR support<br>**Forced** to OFF if `BUILD_CXX=OFF` |
| `BUILD_CoinUtils`  | OFF\* | Static build the CoinUtils library<br>**Forced** to ON if `USE_COINOR=ON` **and** `BUILD_DEPS=ON` |
| `BUILD_Osi`  | OFF\* | Static build the Osi library<br>**Forced** to ON if `USE_COINOR=ON` **and** `BUILD_DEPS=ON` |
| `BUILD_Clp`  | OFF\* | Static build the Clp library<br>**Forced** to ON if `USE_COINOR=ON` **and** `BUILD_DEPS=ON` |
| `BUILD_Cgl`  | OFF\* | Static build the Cgl library<br>**Forced** to ON if `USE_COINOR=ON` **and** `BUILD_DEPS=ON` |
| `BUILD_Cbc`  | OFF\* | Static build the Cbc library<br>**Forced** to ON if `USE_COINOR=ON` **and** `BUILD_DEPS=ON` |
| | | |
| `USE_GLPK`   | OFF\* | Enable GLPK support<br>**Forced** to OFF if `BUILD_CXX=OFF` |
| `BUILD_GLPK` | OFF\* | Static build the GLPK libraries<br>**Forced** to ON if `USE_GLPK=ON` **and** `BUILD_DEPS=ON` |
| | | |
| `USE_HIGHS`  | ON\* | Enable HIGHS support<br>**Forced** to OFF if `BUILD_CXX=OFF` |
| `BUILD_HIGHS` | OFF\* | Static build the HiGHS libraries<br>**Forced** to ON if `USE_HIGHS=ON` **and** `BUILD_DEPS=ON` |
| | | |
| `USE_SCIP`   | ON\*  | Enable SCIP support<br>**Forced** to OFF if `BUILD_CXX=OFF` |
| `BUILD_SCIP` | OFF\* | Static build the SCIP libraries<br>**Forced** to ON if `USE_SCIP=ON` **and** `BUILD_DEPS=ON` |
| | | |
| `USE_CPLEX`  | OFF | Enable CPLEX support |
| | | |
| `BUILD_DOC`   | OFF\* | Build all documentations |
| `BUILD_CXX_DOC` | OFF\* | Build C++ documentation<br>**Forced** to ON if `BUILD_DOC=ON` |
| `BUILD_DOTNET_DOC` | OFF\* | Build .Net documentation<br>**Forced** to ON if `BUILD_DOC=ON` |
| `BUILD_JAVA_DOC` | OFF\* | Build Java documentation<br>**Forced** to ON if `BUILD_DOC=ON` |
| `BUILD_PYTHON_DOC` | OFF\* | Build Python documentation<br>**Forced** to ON if `BUILD_DOC=ON` |
| `INSTALL_DOC` | OFF\* | Install all documentations<br>**Forced** to OFF if `BUILD_CXX=OFF` or `BUILD_DOC=OFF` |
| | | |
| `BUILD_SAMPLES`  | ON\* | Build all samples<br>Default to ON if `BUILD_DEPS=ON` |
| `BUILD_CXX_SAMPLES`  | ON\* | Build all C++ samples<br>**Forced** to OFF if `BUILD_CXX=OFF` or `BUILD_SAMPLE=OFF` |
| `BUILD_DOTNET_SAMPLES`  | ON\* | Build all .Net samples<br>**Forced** to OFF if `BUILD_DOTNET=OFF` or `BUILD_SAMPLE=OFF` |
| `BUILD_JAVA_SAMPLES`  | ON\* | Build all Java samples<br>**Forced** to OFF if `BUILD_JAVA=OFF` or `BUILD_SAMPLE=OFF` |
| `BUILD_PYTHON_SAMPLES`  | ON\* | Build all Python samples<br>**Forced** to OFF if `BUILD_PYTHON=OFF` or `BUILD_SAMPLE=OFF` |
| | | |
| `BUILD_EXAMPLES`  | ON\* | Build all examples<br>Default to ON if `BUILD_DEPS=ON` |
| `BUILD_CXX_EXAMPLES`  | ON\* | Build all C++ examples<br>**Forced** to OFF if `BUILD_CXX=OFF` or `BUILD_SAMPLE=OFF` |
| `BUILD_DOTNET_EXAMPLES`  | ON\* | Build all .Net examples<br>**Forced** to OFF if `BUILD_DOTNET=OFF` or `BUILD_SAMPLE=OFF` |
| `BUILD_JAVA_EXAMPLES`  | ON\* | Build all Java examples<br>**Forced** to OFF if `BUILD_JAVA=OFF` or `BUILD_SAMPLE=OFF` |
| `BUILD_PYTHON_EXAMPLES`  | ON\* | Build all Python examples<br>**Forced** to OFF if `BUILD_PYTHON=OFF` or `BUILD_SAMPLE=OFF` |
| | | |
| `USE_DOTNET_46`  | OFF | Enable .Net Framework 4.6 support<br>Only available if `BUILD_DOTNET=ON` |
| `USE_DOTNET_461`  | OFF | Enable .Net Framework 4.6.1 support<br>Only available if `BUILD_DOTNET=ON` |
| `USE_DOTNET_462`  | OFF | Enable .Net Framework 4.6.2 support<br>Only available if `BUILD_DOTNET=ON` |
| `USE_DOTNET_48`  | OFF | Enable .Net Framework 4.8 support<br>Only available if `BUILD_DOTNET=ON` |
| `USE_DOTNET_STD_21`  | OFF | Enable .Net Standard 2.1 support<br>Only available if `BUILD_DOTNET=ON` and not targeting arm64 platform |
| `USE_DOTNET_CORE_31`  | OFF | Enable .Net Core 3.1 LTS support<br>Only available if `BUILD_DOTNET=ON` and not targeting arm64 platform |
| `USE_DOTNET_6`  | ON | Enable .Net 6 LTS support<br>Only available if `BUILD_DOTNET=ON` |
| `USE_DOTNET_7`  | OFF | Enable .Net 7 support<br>Only available if `BUILD_DOTNET=ON` |
| `UNIVERSAL_DOTNET_PACKAGE`  | OFF | Build a multi platform package (i.e. `Google.OrTools` will depends on all runtime packages)<br>Only available if `BUILD_DOTNET=ON` |
| | | |
| `SKIP_GPG`  | ON | Disable GPG sign<br>Only available if `BUILD_JAVA=ON` |
| `UNIVERSAL_JAVA_PACKAGE`  | OFF | Build a multi platform package (i.e. `ortools-java` will depends on all native packages)<br>Only available if `BUILD_JAVA=ON` |
| `BUILD_FAT_JAR`  | OFF | Build a `ortools-java` .jar that includes all of its own Maven dependencies, including the native package<br>Only available if `BUILD_JAVA=ON` |
| | | |
| `BUILD_pybind11` | `BUILD_DEPS` | Static build the pybind11 libraries<br>**Forced** to ON if `BUILD_DEPS=ON`<br>Only available if `BUILD_PYTHON=ON` |
| `BUILD_pybind11_protobuf` | `BUILD_DEPS` | Static build the pybind11_protobuf libraries<br>**Forced** to ON if `BUILD_DEPS=ON`<br>Only available if `BUILD_PYTHON=ON` |
| `GENERATE_PYTHON_STUB` | ON | Generate python stub files<br>Only available if `BUILD_PYTHON=ON` |
| `BUILD_VENV` | `BUILD_TESTING` | Create python venv in `BINARY_DIR/python/venv`<br>**Forced** to ON if `BUILD_TESTING=ON`<br>Only available if `BUILD_PYTHON=ON` |
| `VENV_USE_SYSTEM_SITE_PACKAGES` | OFF | Python venv can use system site package (e.g. `py3-numpy` on Alpine)<br>Only available if `BUILD_PYTHON=ON` and `BUILD_VENV=ON` |
| `FETCH_PYTHON_DEPS`  | `BUILD_DEPS` | Fetch python modules needed to build ortools package<br>Only available if `BUILD_PYTHON=ON` |
| | | |

## Integrating OR-Tools in your CMake Project

You should be able to integrate OR-Tools in your C++ CMake project following one
of these methods.

For API/ABI compatibility reasons, if you will be using OR-Tools inside a larger
C++ project, we recommend using CMake and incorporate OR-Tools as a CMake
subproject (i.e. using `add_sudirectory()` or `FetchContent`).

### Consuming OR-Tools in a CMake Project

If you already have OR-Tools installed in your system, you can use the CMake
command
[`find_package()`](https://cmake.org/cmake/help/latest/command/find_package.html)
to include OR-Tools in your C++ CMake Project.

note: You may need to set
[`CMAKE_PREFIX_PATH`](https://cmake.org/cmake/help/latest/command/find_package.html#search-procedure)
in order for CMake to find your OR-Tools installation.

```cmake
cmake_minimum_required(VERSION 3.14)
project(myproj VERSION 1.0)

find_package(ortools CONFIG REQUIRED)

add_executable(myapp main.cpp)
target_link_libraries(myapp ortools::ortools)
```

Include directories, compile definitions and compile options will be added
automatically to your target as needed.

### Incorporating OR-Tools into a CMake Super Project

#### Using add_subdirectory

The recommendations below are similar to those for using CMake
[within the googletest framework](https://github.com/google/googletest/blob/main/googletest/README.md#incorporating-into-an-existing-cmake-project)

Thus, you can use the CMake command
[`add_subdirectory()`](https://cmake.org/cmake/help/latest/command/add_subdirectory.html)
to include OR-Tools directly from a subdirectory of your C++ CMake project.<br>
Note: The **ortools::ortools** target is in this case an ALIAS library target
for the **ortools** library target.

```cmake
cmake_minimum_required(VERSION 3.18)
project(myproj VERSION 1.0)

add_subdirectory(or-tools)

add_executable(myapp main.cpp)
target_link_libraries(myapp ortools::ortools)
```

Again, include directories, compile definitions and compile options will be
added automatically to your target as needed.

#### Using FetchContent

If you have `CMake >= 3.18` you can use the built-in module
[`FetchContent`](https://cmake.org/cmake/help/latest/module/FetchContent.html)
instead. Note: The **ortools::ortools** target is in this case an ALIAS library
target for the **ortools** library target.

```cmake
cmake_minimum_required(VERSION 3.18)
project(myproj VERSION 1.0 LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(
  or-tools
  GIT_REPOSITORY https://github.com/google/or-tools.git
  GIT_TAG        main
)

# After the following call, the CMake targets defined by OR-Tools
# will be defined and available to the rest of the build
FetchContent_MakeAvailable(or-tools)

add_executable(myapp main.cpp)
target_link_libraries(myapp ortools::ortools)
```

note: You may need to use the option `-DBUILD_DEPS=ON` to get all the OR-Tools
dependencies as well.
