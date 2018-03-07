# Google OrTools - F# 

## Compiling a standalone binary
This command must be run from the root folder of the repository:
```shell
fsharpc --target:library --out:bin/Google.OrTools.FSharp.dll --platform:anycpu --lib:bin --nocopyfsharpcore --keyfile:bin/keyfile.snk -r:Google.OrTools.dll ortools/fsharp/Google.OrTools.FSharp.fsx
```
For debug information add the `--debug` flag. The library must be coupled with the `Google.OrTools.dll`. Once installed it can be used as follows:
```fsharp
#r "Google.OrTools.dll"
#r "Google.OrTools.Fsharp.dll"

open System
open Google.OrTools.FSharp

let opts = SolverOpts.Default
            .Name("Equality Constraints")
            .Goal(Minimize)
...
```

One can also use the makefile found in the root folder to accomplish the same task.
```shell
make fsharp
```
To see the targets type `make fsharp-help`. Note that a keyfile must exist in the `bin` folder as it will be used to sign the assembly.

## Building Nuget package
Ensure nuget executable is installed and then from root folder run the following:
```shell
make fsharp-build-nuget
```
The output package will include the FSharp binary and examples. It is compiled against the `netstandard2.0` metaframework.
