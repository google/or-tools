# OR-Tools CMake Build Instructions

OR-Tools comes with a CMake build script ([CMakeLists.txt](../CMakeLists.txt))
that can be used on a wide range of platforms (the "C" stands for
cross-platform). If you don't have CMake installed already, you can download it
for free from <http://www.cmake.org/>.

CMake works by generating native Makefiles or build projects that can be used in
the compiler environment of your choice.<br>You can either build OR-Tools with
CMake as a standalone project or it can be incorporated into an existing CMake
 project.

**warning: Currently OR-Tools CMake doesn't support Java nor C#.**

## Table of Contents

* [Building OR-Tools with CMake](#building-or-tools-with-cmake)
* [Consuming OR-Tools in a CMake Project](#consuming-or-tools-in-a-cmake-project)
* [Incorporating OR-Tools into a CMake Project](#incorporating-or-tools-into-a-cmake-project)

For API/ABI compatibility reasons, if you will be using OR-Tools inside a larger
C++ project, we recommend using CMake and incorporate OR-Tools as a CMake
subproject (i.e. using `add_sudirectory()`).

## Building OR-Tools with CMake

When building OR-Tools as a standalone project on Unix-like systems with GNU
Make, the typical workflow is:

1.  Get the source code and change to it.
```sh
git clone https://github.com/google/or-tools.git
cd or-tools
```

2.  Run CMake to configure the build tree.
```sh
cmake -H. -Bbuild -G "Unix Makefiles"
```
note: To get the list of available generators (e.g. Visual Studio), use `-G ""`

3.  Afterwards, generated files can be used to compile the project.
```sh
cmake --build build
```

4.  Test the build software (optional).
```sh
cmake --build build --target test
```

5.  Install the built files (optional).
```sh
cmake --build build --target install
```

## Consuming OR-Tools in a CMake Project

If you already have OR-Tools installed in your system, you can use the CMake
command
[`find_package()`](https://cmake.org/cmake/help/latest/command/find_package.html)
to include OR-Tools in your C++ CMake Project.

```cmake
cmake_minimum_required(VERSION 3.0.2)
project(myproj VERSION 1.0)

find_package(ortools CONFIG REQUIRED)

add_executable(myapp main.cpp)
target_link_libraries(myapp ortools::ortools)
```

Include directories, compile definitions and compile options will be added
automatically to your target as needed.

## Incorporating OR-Tools into a CMake Project

The recommendations below are similar to those for using CMake within the
googletest framework
(<https://github.com/google/googletest/blob/master/googletest/README.md#incorporating-into-an-existing-cmake-project>)

Thus, you can use the CMake command
[`add_subdirectory()`](https://cmake.org/cmake/help/latest/command/add_subdirectory.html)
to include OR-Tools directly from a subdirectory of your C++ CMake project.<br>
Note: The **ortools::ortools** target is in this case an ALIAS library target
for the **ortools** library target.

```cmake
cmake_minimum_required(VERSION 3.0.2)
project(myproj VERSION 1.0)

add_subdirectory(or-tools)

add_executable(myapp main.cpp)
target_link_libraries(myapp ortools::ortools)
```

Again, include directories, compile definitions and compile options will be
added automatically to your target as needed.
