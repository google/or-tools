# Introduction

This file describes how to use OR-Tools in C++ with the binary archive.

OR-Tools online documentation is located at
https://developers.google.com/optimization

Full installation instructions are located at
https://developers.google.com/optimization/install/cpp/

These modules have been tested under:

  - AlmaLinux 9 (64-bit).
  - Alpine Edge (64-bit).
  - ArchLinux (64-bit).
  - Debian 11 and up (64-bit).
  - Fedora 40 and up (64-bit).
  - OpenSuse Leap (64-bit).
  - Rocky Linux 9 (64-bit)
  - Ubuntu 20.04 LTS and up (64-bit).
  - MacOS 12.5 Monterey (64-bit).
  - Microsoft Windows with Visual Studio 2019 and 2022 (64-bit)

## Codemap

Upon decompressing the archive, you will get the following structure:

```
or-tools_cpp/
  [README.md](README.md) <- This file.
  [bin](bin)             <- directory containing executable files.
  [include](include)     <- directory containing headers files.
  [lib](lib)             <- directory containing libraries files.
  [share](share)         <- directory containing various files.
  [examples](examples)   <- C++ examples.
  [Makefile](Makefile)   <- Main Makefile for C++.
```

Warning: While OR-Tools ships with SCIP, please consult the SCIP license
to ensure that you are complying with it if you want to use this solver.

## Usage

Running the examples will involve compiling them before being able to run them.
Here are some ways to compile and run
`examples/basic_example/basic_example.cpp`.

*   Use the provided Makefile to run the `basic_example` example:

```shell
make run SOURCE=examples/basic_example/basic_example.cpp
```

*   To build the same example with CMake, open the folder in the command prompt
    and type the following commands:

    *   On Unix systems:

```shell
cd examples/basic_example
cmake -S . -B build -DCMAKE_PREFIX_PATH=../..
cmake --build build -v
```

    *   On Windows:

```shell
cd examples/basic_example
cmake -S . -B build -DCMAKE_PREFIX_PATH=../..
cmake --build build -v
```
