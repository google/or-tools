# Introduction

This file describes how to use OR-Tools in .Net with the binary archives.

OR-Tools online documentation is
[here](https://developers.google.com/optimization)

Full installation instructions are located
[here](https://developers.google.com/optimization/install/dotnet/)

These modules have been tested under:

  - Ubuntu 20.04 LTS (64 bit).
  - CentOS 8 (64 bit).
  - Debian 10 (64 bit).
  - MacOS Catalina with Xcode 10.x (64 bit).
  - Microsoft Windows with Visual Studio 2019 (64-bit)

Upon decompressing the archive, you will get the following structure:

```
or-tools/
  [LICENSE-2.0.txt](LICENSE-2.0.txt)     <- Apache License
  [README.md](README.md)                 <- This file
  [examples/data/](examples/data)        <- Data for the examples
  [examples/dotnet](examples/dotnet)     <- .Net examples
```

To build an example, open the folder in the command prompt and type the following commands:

```sh
dotnet build
```

To run an example, open the folder in the command prompt and type the following commands:

```sh
dotnet run
```
