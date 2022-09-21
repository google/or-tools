# Introduction

This file describes how to use OR-Tools in Java with the binary archives.

OR-Tools online documentation is located at https://developers.google.com/optimization

Full installation instructions are located at https://developers.google.com/optimization/install/java/

These modules have been tested under:

  - CentOS 7 LTS and Stream 8 (64 bit).
  - Debian 10 and 11 (64 bit).
  - Fedora 33 and up (64 bit).
  - Opensuse Leap (64 bit).
  - Ubuntu 18.04 LTS and up (64 bit).
  - MacOS 12.5 Monterey (64 bit).
  - Microsoft Windows with Visual Studio 2019 and 2022 (64-bit)

## Codemap

Upon decompressing the archive, you will get the following structure:

```
or-tools/
  [LICENSE](LICENSE)     <- Apache License.
  [README.md](README.md) <- This file.
  [examples](examples)   <- Java examples.
  [Makefile](Makefile)   <- Main Makefile for Java.
```

Warning: While OR-Tools ships with SCIP, please consult the SCIP license
to ensure that you are complying with it if you want to use this solver.

## Usage

Running the examples will involve compiling them, then running them.
We have provided a makefile target to help you.

Use Makefile:

```shell
make run SOURCE=examples/BasicExample/BasicExample.java
```

To build an example, open the folder in the command prompt and type the following commands:

**OR** this is equivalent to compiling and running
`examples/BasicExample/BasicExample.java`.

```sh
cd examples/BasicExample
mvn compile -B
mvn run
```
