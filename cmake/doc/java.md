| Linux | macOS | Windows |
|-------|-------|---------|
| [![Status][linux_java_svg]][linux_java_link] | [![Status][macos_java_svg]][macos_java_link] | [![Status][windows_java_svg]][windows_java_link] |

[linux_java_svg]: https://github.com/google/or-tools/actions/workflows/cmake_linux_java.yml/badge.svg?branch=master
[linux_java_link]: https://github.com/google/or-tools/actions/workflows/cmake_linux_java.yml
[macos_java_svg]: https://github.com/google/or-tools/actions/workflows/cmake_macos_java.yml/badge.svg?branch=master
[macos_java_link]: https://github.com/google/or-tools/actions/workflows/cmake_macos_java.yml
[windows_java_svg]: https://github.com/google/or-tools/actions/workflows/cmake_windows_java.yml/badge.svg?branch=master
[windows_java_link]: https://github.com/google/or-tools/actions/workflows/cmake_windows_java.yml

# Introduction

First, verify you have the `JAVA_HOME` environment variable set otherwise CMake and Maven won't be able to find your Java SDK.

## Build the Binary Package

To build the java maven packages, simply run:
```sh
cmake -S. -Bbuild -DBUILD_JAVA=ON
cmake --build build --target java_package -v
```
note: Since `java_package` is in target `all`, you can also ommit the
`--target` option.

## Testing

To list tests:

```sh
cd build
ctest -N
```

To only run Java tests:

```sh
cd build
ctest -R "java_.*"
```

## Technical Notes

First you should take a look at the [ortools/java/README.md](../../ortools/java/README.md) to understand the layout.  
Here I will only focus on the CMake/SWIG tips and tricks.

### Build directory layout

Since Java use the directory layout and we want to use the [CMAKE_BINARY_DIR](https://cmake.org/cmake/help/latest/variable/CMAKE_BINARY_DIR.html) 
to generate the Java binary package.  

We want this layout:

```shell
<CMAKE_BINARY_DIR>/java
├── ortools-linux-x86-64
│   ├── pom.xml
│   └── src/main/resources
│       └── linux-x86-64
│           ├── libjniortools.so
│           └── libortools.so
├── ortools-java
│   ├── pom.xml
│   └── src/main/java
│       └── com/google/ortools
│           ├── Loader.java
│           ├── linearsolver
│           │   ├── LinearSolver.java
│           │   └── ...
│           ├── constraintsolver
│           │   ├── ConstraintSolver.java
│           │   └── ...
│           ├── ... 
│           └── sat
│               ├── CpModel.java
│               └── ...
├── constraint_solver
│   └── Tsp
│       ├── pom.xml
│       └── src/main/java
│           └── com/google/ortools
│               └── Tsp.java
```
src: `tree build/java --prune -U -P "*.java|*.xml|*.so*" -I "target"`

### Managing SWIG generated files

You can use `CMAKE_SWIG_DIR` to change the output directory for the `.java` file e.g.:

```cmake
set(CMAKE_SWIG_OUTDIR ${CMAKE_CURRENT_BINARY_DIR}/..)
```
And you can use `CMAKE_LIBRARY_OUTPUT_DIRECTORY` to change the output directory for the `.so` file e.g.:

```cmake
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/..)
```
[optional]You can use `SWIG_OUTFILE_DIR` to change the output directory for the `.cxx` file e.g.:

```cmake
set(SWIG_OUTFILE_DIR ${CMAKE_CURRENT_BINARY_DIR}/..)
```
Then you only need to create a `pom.xml` file in `build/java` to be able to use
the build directory to generate the Java package.
