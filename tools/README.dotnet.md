# Introduction

This file describes how to use OR-Tools in .Net with the binary archives.

OR-Tools online documentation is
[here](https://developers.google.com/optimization)

Full installation instructions are located
[here](https://developers.google.com/optimization/install/dotnet/)

These modules have been tested under:

  - CentOS 8 (64 bit).
  - Debian 11 (64 bit).
  - Fedora 33 and up (64 bit).
  - Ubuntu 18.04 LTS and up (64 bit).
  - MacOS 12.2 Monterey (64 bit).
  - Microsoft Windows with Visual Studio 2022 (64-bit)

Upon decompressing the archive, you will get the following structure:

```
or-tools/
  [LICENSE](LICENSE)                 <- Apache License
  [README.md](README.md)             <- This file
  [examples/data/](examples/data)    <- Data for the examples
  [examples/dotnet](examples/dotnet) <- .Net examples
```

To build an example, open the folder in the command prompt and type the following commands:

```sh
dotnet build
```

To run an example, open the folder in the command prompt and type the following commands:

```sh
dotnet run
```
