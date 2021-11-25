# OR-Tools CMake Build Instructions
| OS       | C++   | Python   | Java   | .NET   |
|:-------- | :---: | :------: | :----: | :----: |
| Linux    | [![Status][linux_cpp_svg]][linux_cpp_link] | [![Status][linux_python_svg]][linux_python_link] | [![Status][linux_java_svg]][linux_java_link] | [![Status][linux_dotnet_svg]][linux_dotnet_link] |
| MacOS    | [![Status][macos_cpp_svg]][macos_cpp_link] | [![Status][macos_python_svg]][macos_python_link] | [![Status][macos_java_svg]][macos_java_link] | [![Status][macos_dotnet_svg]][macos_dotnet_link] |
| Windows  | [![Status][windows_cpp_svg]][windows_cpp_link] | [![Status][windows_python_svg]][windows_python_link] | [![Status][windows_java_svg]][windows_java_link] | [![Status][windows_dotnet_svg]][windows_dotnet_link] |

[linux_cpp_svg]: https://github.com/google/or-tools/actions/workflows/cmake_linux_cpp.yml/badge.svg?branch=master
[linux_cpp_link]: https://github.com/google/or-tools/actions/workflows/cmake_linux_cpp.yml
[linux_python_svg]: https://github.com/google/or-tools/actions/workflows/cmake_linux_python.yml/badge.svg?branch=master
[linux_python_link]: https://github.com/google/or-tools/actions/workflows/cmake_linux_python.yml
[linux_java_svg]: https://github.com/google/or-tools/actions/workflows/cmake_linux_java.yml/badge.svg?branch=master
[linux_java_link]: https://github.com/google/or-tools/actions/workflows/cmake_linux_java.yml
[linux_dotnet_svg]: https://github.com/google/or-tools/actions/workflows/cmake_linux_dotnet.yml/badge.svg?branch=master
[linux_dotnet_link]: https://github.com/google/or-tools/actions/workflows/cmake_linux_dotnet.yml

[macos_cpp_svg]: https://github.com/google/or-tools/actions/workflows/cmake_macos_cpp.yml/badge.svg?branch=master
[macos_cpp_link]: https://github.com/google/or-tools/actions/workflows/cmake_macos_cpp.yml
[macos_python_svg]: https://github.com/google/or-tools/actions/workflows/cmake_macos_python.yml/badge.svg?branch=master
[macos_python_link]: https://github.com/google/or-tools/actions/workflows/cmake_macos_python.yml
[macos_java_svg]: https://github.com/google/or-tools/actions/workflows/cmake_macos_java.yml/badge.svg?branch=master
[macos_java_link]: https://github.com/google/or-tools/actions/workflows/cmake_macos_java.yml
[macos_dotnet_svg]: https://github.com/google/or-tools/actions/workflows/cmake_macos_dotnet.yml/badge.svg?branch=master
[macos_dotnet_link]: https://github.com/google/or-tools/actions/workflows/cmake_macos_dotnet.yml

[windows_cpp_svg]: https://github.com/google/or-tools/actions/workflows/cmake_windows_cpp.yml/badge.svg?branch=master
[windows_cpp_link]: https://github.com/google/or-tools/actions/workflows/cmake_windows_cpp.yml
[windows_python_svg]: https://github.com/google/or-tools/actions/workflows/cmake_windows_python.yml/badge.svg?branch=master
[windows_python_link]: https://github.com/google/or-tools/actions/workflows/cmake_windows_python.yml
[windows_java_svg]: https://github.com/google/or-tools/actions/workflows/cmake_windows_java.yml/badge.svg?branch=master
[windows_java_link]: https://github.com/google/or-tools/actions/workflows/cmake_windows_java.yml
[windows_dotnet_svg]: https://github.com/google/or-tools/actions/workflows/cmake_windows_dotnet.yml/badge.svg?branch=master
[windows_dotnet_link]: https://github.com/google/or-tools/actions/workflows/cmake_windows_dotnet.yml

Dockers [Alpine, Archlinux, Centos, Debian, Fedora, OpenSuse, Ubuntu]x[C++, Python, Java, .Net]: [![Status][docker_svg]][docker_link]

[docker_svg]: https://github.com/google/or-tools/actions/workflows/cmake_docker.yml/badge.svg?branch=master
[docker_link]: https://github.com/google/or-tools/actions/workflows/cmake_docker.yml


## Introduction
<nav for="cmake"> |
<a href="#deps">Dependencies</a> |
<a href="#options">Options</a> |
<a href="doc/cpp.md">C++</a> |
<a href="doc/swig.md">Swig</a> |
<a href="doc/python.md">Python 3</a> |
<a href="doc/dotnet.md">.Net Core</a> |
<a href="doc/java.md">Java</a> |
<a href="#integration">Integration</a> |
<a href="doc/ci.md">CI</a> |
</nav>

OR-Tools comes with a CMake based build ([CMakeLists.txt](../CMakeLists.txt))
that can be used on a wide range of platforms (the "C" stands for
cross-platform). If you don't have CMake installed already, you can download it
for free from <http://www.cmake.org/>.

CMake works by generating native Makefiles or build projects that can be used in
the compiler environment of your choice.<br>You can either build OR-Tools with
CMake as a standalone project or it can be incorporated into an existing CMake
 project.

<a name="deps"></a>

## Dependencies

OR-Tools depends on several mandatory libraries. You can compile them all at
configure time using the option `-DBUILD_DEPS=ON` (`OFF` by default) or you can
compile few of them using the options below (see [CMake Options](#cmake-options)
below).

*   ZLIB (`BUILD_ZLIB`),
*   Google Abseil-cpp (`BUILD_absl`),
*   Google Protobuf (`BUILD_Protobuf`),
*   SCIP (`BUILD_SCIP`),<br>
  note: You can disable the support of SCIP solvers
  by using `-DUSE_SCIP=OFF` (`ON` by default).
*   GLPK (`BUILD_GLPK`),<br>
  note: You can disable the support of GLPK solvers
  by using `-DUSE_GLPK=OFF` (`ON` by default).

*   COIN-OR solvers,

    *   COIN-OR CoinUtils (`BUILD_CoinUtils`),
    *   COIN-OR Osi (`BUILD_Osi`),
    *   COIN-OR Clp (`BUILD_Clp`),
    *   COIN-OR Cgl (`BUILD_Cgl`),
    *   COIN-OR Cbc (`BUILD_Cbc`),<br>
  note: You can disable the support of COIN-OR solvers (i.e. Cbc and Clp solver)
  by using `-DUSE_COINOR=OFF` (`ON` by default).

OR-Tools can also optionally (disabled by default) be compiled with support for
the following third-party proprietary solvers:

* CPLEX (`USE_CPLEX`),
* XPRESS (`USE_XPRESS`)

**warning: Since these solvers require license and are proprietary, we can't
test it on public CI and support can be broken.**

### Enabling CPLEX Support

To enable CPLEX support, configure with `-DUSE_CPLEX=ON` and
`-DCPLEX_ROOT=/absolute/path/to/CPLEX/root/dir`, replacing
`/absolute/path/to/CPLEX/root/dir` with the path to your CPLEX installation.
`CPLEX_ROOT` can also be defined as an environment variable rather than an
option at configure time.

For ease of migration from legacy `make third_party` builds, CMake will also
read the CPLEX installation path from the `UNIX_CPLEX_DIR` environment variable,
if defined.

### Enabling XPRESS Support

To enable XPRESS support, configure with `-DUSE_XPRESS=ON` and
`-DXPRESS_ROOT=/absolute/path/to/XPRESS/root/dir`, replacing
`/absolute/path/to/XPRESS/root/dir` with the path to your XPRESS installation.
`XPRESS_ROOT` can also be defined as an environment variable rather than an
option at configure time.

<a name="options"></a>

## CMake Options

There are several options that can be passed to CMake to modify how the code is built.<br>
For all of these options and parameters you have to use `-D<Parameter_name>=<value>`.

All CMake options are passed at configure time, i.e., by running <br>
`cmake -S. -B<your_chosen_build_directory>  -DOPTION_ONE=ON -DOPTION_TWO=OFF ...` <br>
before running `cmake --build <your_chosen_build_directory>`<br>

For example, to generate build files including dependencies in a new subdirectory called 'build', run:
```sh
cmake -S. -Bbuild  -DBUILD_DEPS:BOOL=ON
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
|-|-|-|
| `CMAKE_BUILD_TYPE` | Release | see CMake documentation [here](https://cmake.org/cmake/help/latest/variable/CMAKE_BUILD_TYPE.html) |
| `BUILD_CXX` | ON | Build C++ |
| `BUILD_PYTHON` | OFF | Build Python wrapper and package |
| `BUILD_JAVA` | OFF | Build Java wrapper and packages |
| `BUILD_DOTNET` | OFF | Build .Net wrapper and packages |
| `BUILD_FLATZINC` | ON\* | Build the flatzinc library<br>**Forced** to OFF if `BUILD_CXX=OFF` |
| `BUILD_GLOP` | OFF\* | Build the standalone Glop library<br>**Forced** to OFF if `BUILD_CXX=ON`, otherwise default to ON |
| | | |
| `BUILD_DEPS`  | OFF* | Default to ON if `BUILD_JAVA=ON` or `BUILD_PYTHON=ON` or `BUILD_DOTNET=ON` |
| `BUILD_ZLIB`  | OFF* | Static build the zlib library<br>**Forced** to ON if `BUILD_DEPS=ON` |
| `BUILD_absl`  | OFF* | Static build the abseil-cpp libraries<br>**Forced** to ON if `BUILD_DEPS=ON` |
| `BUILD_Protobuf`  | OFF* | Static build the protobuf libraries<br>**Forced** to ON if `BUILD_DEPS=ON` |
| `USE_SCIP`  | ON\* | Enable SCIP support<br>**Forced** to OFF if `BUILD_CXX=OFF` |
| `BUILD_SCIP`  | OFF\* | Static build the SCIP libraries<br>**Forced** to ON if `USE_SCIP=ON` **and** `BUILD_DEPS=ON` |
| `USE_GLPK`  | ON\* | Enable GLPK support<br>**Forced** to OFF if `BUILD_CXX=OFF` |
| `BUILD_GLPK`  | OFF\* | Static build the GLPK libraries<br>**Forced** to ON if `USE_GLPK=ON` **and** `BUILD_DEPS=ON` |
| `USE_COINOR`  | ON\* | Enable Coin-OR support<br>**Forced** to OFF if `BUILD_CXX=OFF` |
| `BUILD_CoinUtils`  | OFF\* | Static build the CoinUtils library<br>**Forced** to ON if `USE_COINOR=ON` **and** `BUILD_DEPS=ON` |
| `BUILD_Osi`  | OFF\* | Static build the Osi library<br>**Forced** to ON if `USE_COINOR=ON` **and** `BUILD_DEPS=ON` |
| `BUILD_Clp`  | OFF\* | Static build the Clp library<br>**Forced** to ON if `USE_COINOR=ON` **and** `BUILD_DEPS=ON` |
| `BUILD_Cgl`  | OFF\* | Static build the Cgl library<br>**Forced** to ON if `USE_COINOR=ON` **and** `BUILD_DEPS=ON` |
| `BUILD_Cbc`  | OFF\* | Static build the Cbc library<br>**Forced** to ON if `USE_COINOR=ON` **and** `BUILD_DEPS=ON` |
| `USE_CPLEX`  | OFF | Enable CPLEX support |
| `USE_XPRESS`  | OFF | Enable XPRESS support |
| | | |
| `BUILD_SAMPLES`  | OFF\* | Build all samples<br>Default to ON if `BUILD_DEPS=ON` |
| `BUILD_CXX_SAMPLES`  | ON\* | Build all C++ samples<br>**Forced** to OFF if `BUILD_CXX=OFF` or `BUILD_SAMPLE=OFF` |
| `BUILD_PYTHON_SAMPLES`  | ON\* | Build all Python samples<br>**Forced** to OFF if `BUILD_PYTHON=OFF` or `BUILD_SAMPLE=OFF` |
| `BUILD_JAVA_SAMPLES`  | ON\* | Build all Java samples<br>**Forced** to OFF if `BUILD_JAVA=OFF` or `BUILD_SAMPLE=OFF` |
| `BUILD_DOTNET_SAMPLES`  | ON\* | Build all .Net samples<br>**Forced** to OFF if `BUILD_DOTNET=OFF` or `BUILD_SAMPLE=OFF` |
| | | |
| `BUILD_EXAMPLES`  | OFF\* | Build all examples<br>Default to ON if `BUILD_DEPS=ON` |
| `BUILD_CXX_EXAMPLES`  | ON\* | Build all C++ examples<br>**Forced** to OFF if `BUILD_CXX=OFF` or `BUILD_SAMPLE=OFF` |
| `BUILD_PYTHON_EXAMPLES`  | ON\* | Build all Python examples<br>**Forced** to OFF if `BUILD_PYTHON=OFF` or `BUILD_SAMPLE=OFF` |
| `BUILD_JAVA_EXAMPLES`  | ON\* | Build all Java examples<br>**Forced** to OFF if `BUILD_JAVA=OFF` or `BUILD_SAMPLE=OFF` |
| `BUILD_DOTNET_EXAMPLES`  | ON\* | Build all .Net examples<br>**Forced** to OFF if `BUILD_DOTNET=OFF` or `BUILD_SAMPLE=OFF` |
| | | |
| `SKIP_GPG`  | OFF | Disable GPG sign<br>Only available if `BUILD_JAVA=ON` |
| `UNIVERSAL_JAVA_PACKAGE`  | OFF | Build a multi platform package (i.e. `ortools-java` will depends on all native packages)<br>Only available if `BUILD_JAVA=ON` |
| `BUILD_FAT_JAR`  | OFF | Build a `ortools-java` .jar that includes all of its own Maven dependencies, including the native package<br>Only available if `BUILD_JAVA=ON` |
| | | |
| `FETCH_PYTHON_DEPS`  | BUILD_DEPS | Fetch python modules needed to build ortools package<br>Only available if `BUILD_PYTHON=ON` |
| | | |

<a name="integration"></a>

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

The recommendations below are similar to those for using CMake within the
googletest framework
(<https://github.com/google/googletest/blob/master/googletest/README.md#incorporating-into-an-existing-cmake-project>)

Thus, you can use the CMake command
[`add_subdirectory()`](https://cmake.org/cmake/help/latest/command/add_subdirectory.html)
to include OR-Tools directly from a subdirectory of your C++ CMake project.<br>
Note: The **ortools::ortools** target is in this case an ALIAS library target
for the **ortools** library target.

```cmake
cmake_minimum_required(VERSION 3.14)
project(myproj VERSION 1.0)

add_subdirectory(or-tools)

add_executable(myapp main.cpp)
target_link_libraries(myapp ortools::ortools)
```

Again, include directories, compile definitions and compile options will be
added automatically to your target as needed.

#### Using FetchContent

If you have `CMake >= 3.14.7` you can use the built-in module
[FetchContent](https://cmake.org/cmake/help/latest/module/FetchContent.html)
instead. Note: The **ortools::ortools** target is in this case an ALIAS library
target for the **ortools** library target.

```cmake
cmake_minimum_required(VERSION 3.14)
project(myproj VERSION 1.0 LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(
  or-tools
  GIT_REPOSITORY https://github.com/google/or-tools.git
  GIT_TAG        master
)

# After the following call, the CMake targets defined by or-tools
# will be defined and available to the rest of the build
FetchContent_MakeAvailable(or-tools)

add_executable(myapp main.cpp)
target_link_libraries(myapp ortools::ortools)
```

note: You may need to use the option `-DBUILD_DEPS=ON` to get all or-tools
dependencies as well.
