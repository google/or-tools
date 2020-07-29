| Linux | macOS | Windows |
|-------|-------|---------|
| [![Status][java_linux_svg]][java_linux_link] | [![Status][java_osx_svg]][java_osx_link] | [![Status][java_win_svg]][java_win_link] |

[java_linux_svg]: https://github.com/google/or-tools/workflows/Java%20Linux%20CI/badge.svg
[java_linux_link]: https://github.com/google/or-tools/actions?query=workflow%3A"Java+Linux+CI"
[java_osx_svg]: https://github.com/google/or-tools/workflows/Java%20MacOS%20CI/badge.svg
[java_osx_link]: https://github.com/google/or-tools/actions?query=workflow%3A"Java+MacOS+CI"
[java_win_svg]: https://github.com/google/or-tools/workflows/Java%20Windows%20CI/badge.svg
[java_win_link]: https://github.com/google/or-tools/actions?query=workflow%3A"Java+Windows+CI"

# Introduction


# Build the Binary Package
To build the java package, simply run:
```sh
cmake -S. -Bbuild -DBUILD_JAVA=ON
cmake --build build
cmake --build build --target java_package
```

## Build directory layout
Since Java use the directory layout and we want to use the [CMAKE_BINARY_DIR](https://cmake.org/cmake/help/latest/variable/CMAKE_BINARY_DIR.html) 
to generate the Java binary package.  

We want this layout (`tree build --prune -U -P "*.java|*.so*" -I "build"`):
```shell
build/java/com/google/ortools
```

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

# Testing Java
TODO
