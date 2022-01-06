| Linux | macOS | Windows |
|-------|-------|---------|
| [![Status][linux_cpp_svg]][linux_cpp_link] | [![Status][macos_cpp_svg]][macos_cpp_link] | [![Status][windows_cpp_svg]][windows_cpp_link] |

[linux_cpp_svg]: https://github.com/google/or-tools/actions/workflows/cmake_linux_cpp.yml/badge.svg?branch=master
[linux_cpp_link]: https://github.com/google/or-tools/actions/workflows/cmake_linux_cpp.yml
[macos_cpp_svg]: https://github.com/google/or-tools/actions/workflows/cmake_macos_cpp.yml/badge.svg?branch=master
[macos_cpp_link]: https://github.com/google/or-tools/actions/workflows/cmake_macos_cpp.yml
[windows_cpp_svg]: https://github.com/google/or-tools/actions/workflows/cmake_windows_cpp.yml/badge.svg?branch=master
[windows_cpp_link]: https://github.com/google/or-tools/actions/workflows/cmake_windows_cpp.yml

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
cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=Release -DBUILD_DEPS=ON
```
note: To get the list of available generators (e.g. Visual Studio), use `-G ""`

3.  Afterwards, generated files can be used to compile the project.
```sh
cmake --build build --config Release -v
```

4.  Test the build software (optional).
```sh
cmake --build build --target test
```

5.  Install the built files (optional).
```sh
cmake --build build --target install
```

# Testing
To list tests:
```sh
cd build
ctest -N
```

To only run C++ tests
```sh
cd build
ctest -R "cxx_.*"
```

# Technical Notes
## Managing RPATH
Since we want to use the [CMAKE_BINARY_DIR](https://cmake.org/cmake/help/latest/variable/CMAKE_BINARY_DIR.html) to generate the wrapper package (e.g. python wheel package) as well as be able to test from the build directory.
We need to enable:
```cmake
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
```
And have a finely tailored rpath for each library.
