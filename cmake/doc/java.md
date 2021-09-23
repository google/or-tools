| Linux | macOS | Windows |
|-------|-------|---------|
| [![Status][java_linux_svg]][java_linux_link] | [![Status][java_osx_svg]][java_osx_link] | [![Status][java_win_svg]][java_win_link] |

[java_linux_svg]: https://github.com/google/or-tools/workflows/Java%20Linux%20CI/badge.svg?branch=master
[java_linux_link]: https://github.com/google/or-tools/actions?query=workflow%3A"Java+Linux+CI"
[java_osx_svg]: https://github.com/google/or-tools/workflows/Java%20MacOS%20CI/badge.svg?branch=master
[java_osx_link]: https://github.com/google/or-tools/actions?query=workflow%3A"Java+MacOS+CI"
[java_win_svg]: https://github.com/google/or-tools/workflows/Java%20Windows%20CI/badge.svg?branch=master
[java_win_link]: https://github.com/google/or-tools/actions?query=workflow%3A"Java+Windows+CI"

# Introduction

# Note for Mac Users
If you installed `java` through `brew` you will need to export the `JAVA_HOME` variable. 

# Build the Binary Package
To build the java maven packages, simply run:
```sh
cmake -S. -Bbuild -DBUILD_JAVA=ON
cmake --build build --target java_package -v
```
note: Since `java_package` is in target `all`, you can also ommit the
`--target` option.

# Testing
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

# Technical Notes
First you should take a look at the [ortools/java/README.md](../../ortools/java/README.md) to understand the layout.  
Here I will only focus on the CMake/SWIG tips and tricks.

## Build directory layout
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

## Managing SWIG generated files
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
