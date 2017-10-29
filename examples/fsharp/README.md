# Examples of using or-tools in FSharp

Examples from the or-tools library utilizing F#

## SolverOptions and lpSolve
This function and parameter object are a wrapper around the standard or-tools functions. It is designed
to enter the Linear/Integer program as matrices & vectors. Two input formats are allowed: Canonical Form; Standard Form.

*__ALL Matrices & Vectors are entered as columns__*

## Execution
Be sure to compile the or-tools before executing following
```shell
fsharpc --target:exe --out:bin/<example_file>.exe --platform:anycpu --lib:bin -r:Google.OrTools.dll examples/fsharp/<example_file>.fsx

DYLD_FALLBACK_LIBRARY_PATH=lib mono bin/<example_file>.exe

```

## Compiling a standalone binary

```shell
fsharpc --target:library --out:bin/Google.OrTools.FSharp.dll --platform:anycpu --lib:bin -r:Google.OrTools.dll examples/fsharp/lib/Google.OrTools.FSharp.fsx
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



