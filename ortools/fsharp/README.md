# Google OrTools - F# 

## Pre-requisites
- dotnet core 2.0
- Mono framework 5.4+

## Compiling a standalone binary

Use the makefile found in the root folder. IT to accomplish the same task.
```shell
make fsharp
```
To see the targets type `make help_fsharp`. Note that a keyfile must exist in the `bin` folder as it will be used to sign the assembly.

## Building Nuget package
Ensure nuget executable is installed and then from root folder run the following:
```shell
make nuget-pkg_fsharp
```
The output package will include the FSharp binary and examples. It contains binaries for  `netstandard2.0` metaframework and `net462`. If using the NETstandard binary you'll need to include a reference in the example to that library on your local system.
