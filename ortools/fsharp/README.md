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
To see the targets type `make help_fsharp`. Note that a keyfile must exist in the `bin` folder as it will be used to sign the assembly.

## Test the Library
Run the following to execute the tests. It requires the `dotnet` CLI tool. It uses the xUnit framework.
```shell
make test_fsharp

xUnit.net Console Runner (64-bit .NET Core 4.6.0.0)
  Discovering: Google.OrTools.FSharp.Test
  Discovered:  Google.OrTools.FSharp.Test
  Starting:    Google.OrTools.FSharp.Test
  Finished:    Google.OrTools.FSharp.Test
=== TEST EXECUTION SUMMARY ===
   Google.OrTools.FSharp.Test  Total: 1, Errors: 0, Failed: 0, Skipped: 0, Time: 0.355s
```

## Building Nuget package
Ensure nuget executable is installed and then from root folder run the following:
```shell
make nuget-pkg_fsharp
```
The output package will include the FSharp binary and examples. It is compiled against the `netstandard2.0` metaframework.
