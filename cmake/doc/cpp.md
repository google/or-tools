| Linux | macOS | Windows |
|-------|-------|---------|
| [![Status][cpp_linux_svg]][cpp_linux_link] | [![Status][cpp_osx_svg]][cpp_osx_link] | [![Status][cpp_win_svg]][cpp_win_link] |


[cpp_linux_svg]: https://github.com/google/or-tools/workflows/C++%20Linux%20CI/badge.svg
[cpp_linux_link]: https://github.com/google/or-tools/actions?query=workflow%3A"C%2B%2B+Linux+CI"
[cpp_osx_svg]: https://github.com/google/or-tools/workflows/C++%20MacOS%20CI/badge.svg
[cpp_osx_link]: https://github.com/google/or-tools/actions?query=workflow%3A"C%2B%2B+MacOS+CI"
[cpp_win_svg]: https://github.com/google/or-tools/workflows/C++%20Windows%20CI/badge.svg
[cpp_win_link]: https://github.com/google/or-tools/actions?query=workflow%3A"C%2B%2B+Windows+CI"

# Introduction
For building OR-Tools as a CMake standalone project, you can read the following instructions.

This project should run on GNU/Linux, MacOS and Windows.

# C++ Project Build
1.  Get the source code and change to it.
```sh
git clone https://github.com/google/or-tools.git
cd or-tools
```

2.  Run CMake to configure the build tree.
```sh
cmake -S. -Bbuild -G "Unix Makefiles" -DBUILD_DEPS=ON
```
note: To get the list of available generators (e.g. Visual Studio), use `-G ""`

3.  Afterwards, generated files can be used to compile the project.
```sh
cmake --build build -v
```

4.  Test the build software (optional).
```sh
cmake --build build --target test
```

5.  Install the built files (optional).
```sh
cmake --build build --target install
```

# Technical Notes
## Managing RPATH
Since we want to use the [CMAKE_BINARY_DIR](https://cmake.org/cmake/help/latest/variable/CMAKE_BINARY_DIR.html) to generate the wrapper package (e.g. python wheel package) as well as be able to test from the build directory.
We need to enable:
```cmake
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
```
And have a finely tailored rpath for each library.
