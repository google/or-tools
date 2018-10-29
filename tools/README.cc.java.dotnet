# Introduction
This file describes how to install the OR-Tools C++, Java and .Net binary archive.

OR-Tools is located at https://developers.google.com/optimization

These modules have been tested under:
  - Ubuntu 14.04, 16.04, 17.10 and 18.04 (64-bit).
  - macOS 10.13 High Sierra with Xcode 9.4 (64 bit).
  - Microsoft Windows with Visual Studio 2015 and 2017 (64-bit)

Upon decompressing the archive, you will get the following structure:

```
or-tools/
  LICENSE-2.0.txt <- Apache 2.0 License
  README.md       <- This file
  Makefile        <- Main Makefile for C++,Java and .Net
  examples/       <- C++, Java and .Net examples
  include/        <- all include files
  objs/           <- directory containing C++ compiled object files (*.o)
  classes/        <- directory containing Java class files.
  packages/       <- directory containing .Net nuget packages.
  lib/            <- directory containing libraries and jar files.
  bin/            <- directory containing executable files
```

# C++

Running the examples will involve compiling them, then running them.
We have provided a makefile target to help you.

Use Makefile:
```shell
make run SOURCE=examples/cpp/golomb.cc
```

**OR** this is equivalent to compiling and running
`examples/cpp/golomb.cc`.
- on Unix:
  ```shell
  make bin/golomb
  ./bin/golomb
  ```
- on Windows:
  ```shell
  make bin\\golomb.exe
  bin\\golomb.exe
  ```

# Java

Running the examples will involve compiling them, then running them.
We have provided a makefile target to help you. You need to have the
java and javac tools accessible from the command line.

Use Makefile:
```shell
make run SOURCE=examples/java/RabbitsPheasants.java
```

**OR** this is equivalent to compiling and running
`examples/java/RabbitsPheasants.java`.
- on Unix:
  ```shell
  javac -d classes/RabbitsPheasants -cp lib/com.google.ortools.jar:lib/protobuf.jar examples/java/RabbitsPheasants.java
  jar cvf lib/RabbitsPheasants.jar -C classes/RabbitsPheasants .
  java -Djava.library.path=lib -cp lib/RabbitsPheasants.jar:lib/com.google.ortools.jar:lib/protobuf.jar RabbitsPheasants
  ```
- on Windows:
  ```shell
  javac -d class/RabbitsPheasants -cp lib/com.google.ortools.jar;lib/protobuf.jar examples/java/RabbitsPheasants.java
  jar cvf lib/RabbitsPheasants.jar -C classes/RabbitsPheasants .
  java -Djava.library.path=lib -cp lib/RabbitPheasants.jar;lib/com.google.ortools.jar;lib/protobuf.jar RabbitsPheasants
  ```

# .Net

Running the examples will involve compiling them, then running them.
We have provided a makefile target to help you. You need to have the
dotnet/cli tools accessible from the command line.

Use Makefile:
```shell
make run SOURCE=examples/dotnet/csflow.cs
```

**OR** this is equivalent to compiling and running
`examples/dotnet/csflow.cs`.
- on Unix:
  ```shell
  dotnet build examples/dotnet/csflow.csproj
  dotnet run --no-build --project examples/dotnet/csflow.csproj
  ```
- on Windows:
  ```shell
  dotnet build examples\dotnet\csflow.csproj
  dotnet run --no-build --project examples\dotnet\csflow.csproj
  ```
