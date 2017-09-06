# Examples of using or-tools in FSharp

Examples from the or-tools library utilizing F#

# Execution
Be sure to compile the or-tools before executing following
```shell
fsharpc --target:exe --out:bin/<example_file>.exe --platform:anycpu --lib:bin -r:Google.OrTools.dll examples/fsharp/<example_file>.fs
DYLD_FALLBACK_LIBRARY_PATH=lib mono64 bin/cslinearprogramming.exe

```
