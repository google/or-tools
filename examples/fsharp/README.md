# Examples of using or-tools in FSharp

Examples from the or-tools library utilizing F#

## SolverOptions and lpSolve
This function and parameter object are a wrapper around the standard or-tools functions. It is designed
to enter the Linear/Integer program as matrices & vectors. Two input formats are allowed: Canonical Form; Standard Form.

*__ALL Matrices & Vectors are entered as columns__*

## Execution
Be sure to compile the or-tools (native & managed F# library) before executing following
```shell
fsharpc --target:exe --out:bin/<example_file>.exe --platform:anycpu --lib:bin examples/fsharp/<example_file>.fsx

DYLD_FALLBACK_LIBRARY_PATH=lib mono bin/<example_file>.exe

```
