# ORToolsBinaries

This package, upon installation, downloads the latest OR-Tools binaries from
GitHub. This is especially useful on platforms like Windows or ARM64/Aarch64
because no JLL is available for them.

This package otherwise provides the same interface as
[`ORTools_jll.jl`](https://github.com/JuliaBinaryWrappers/ORTools_jll.jl),
i.e. a global variable `libortools` that contains the path to the local copy
of OR-Tools.
