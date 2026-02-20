# Julia wrappers

This folder contains the Julia wrappers for OR-Tools. They are not useable from
within Google3: they are intended for use by the open-source community only.

Note: the wrapper is being written and highly experimental.

For now, only MathOpt is accessible through
[its C API](../math_opt/core/c_api).

TODO(user): how to run this on my machine?

TODO(user): generate the Julia files within CMake and export them manually
with Yggdrasil.

## Packages

The Julia wrapper is divided into three Julia packages:

*   [`ORTools_jll`](https://github.com/JuliaRegistries/General/tree/master/jll/O/ORTools_jll)
    contains the precompiled OR-Tools binaries.
*   [`ORToolsGenerated.jl`](../ORToolsGenerated.jl)
    contains the generated part of the wrapper. For now, this package
    corresponds to Protocol Buffers files.
*   [`ORTools.jl`](../ORTools.jl)
    is the actual, high-level wrapper (more specifically, the
    [MathOptInterface](https://github.com/jump-dev/MathOptInterface.jl) code for
    use through [JuMP](https://jump.dev/)). **It is not yet written.**

## Maintenance

When releasing a new version of OR-Tools:

1.  Update the binary dependencies in
    [`ORTools_jll`](https://github.com/JuliaPackaging/Yggdrasil/blob/master/O/ORTools/build_tarballs.jl):
    create a public PR on GitHub that edits `build_tarballs.jl`. At each
    version, you must update two fields:

    *   `version`: to match the version of OR-Tools.
    *   `GitSource()`: to match the Git commit that you want to build.

    Update the rest of the file as required. In particular, you must update the
    installation of the Protocol Buffers files and the list of exported Protocol
    Buffers files any time the new release has new Protocol Buffers files.

2.  Update the generated files in
    [`ORToolsGenerated.jl`](../ORToolsGenerated.jl).
    This step depends on the first one: the PR you wrote at the first step must
    be accepted and merged into the repository. Updating the generated files
    amounts to have a local Julia installation, adding the new version of
    `ORTools_jll` (within a Julia shell, `]add ORTools_jll` the first time, then
    update the version with `]update`), and calling the script
    [`update_package.jl`](../ORToolsGenerated.jl/scripts/update_package.jl).

3.  Synchronize the results of the previous step with OR-Tools' repository: the
    updated files go into `ortools/julia/ORToolsGenerated/` (an updated
    `Project.toml`, new files in `src/gen*`).

4.  Trigger a release of `ORToolsGenerated.jl` using
    [JuliaRegistrator](https://github.com/JuliaRegistries/Registrator.jl). To
    this end:

    *   Find the new commit you created at the previous step in GitHub.
    *   Add the following comment to register the
        `ortools/julia/ORToolsGenerated.jl` as a Julia package:

        ```
        @JuliaRegistrator register subdir=ortools/julia/ORToolsGenerated.jl
        ```

        If registering from a nondefault branch like `julia/dev`:

        ```
        @JuliaRegistrator register subdir=ortools/julia/ORToolsGenerated.jl branch=julia/dev
        ```

    This operation starts an automated process that will publish a new version
    of the `ORToolsGenerated.jl` package to the
    [official Julia package repository](https://juliapackages.com/). The version
    number is taken from the contents of `Project.toml` at this Git commit.

5.  In case the C API changes, you must update `ORTools.jl` accordingly: after
    the manual update, follow the steps 3 and 4, replacing `ORToolsGenerated.jl`
    by `ORTools.jl`. To register the new version, the command thus looks like:

    ```
    @JuliaRegistrator register subdir=ortools/julia/ORTools.jl branch=julia/dev
    ```

There is only one set-up action to perform: install Julia's Registrator bot into
OR-Tools' repository (which only needs read access) using
[their one-click procedure](https://github.com/JuliaComputing/Registrator.jl?tab=readme-ov-file#install-registrator).

### Package folder structure

This folder corresponds to the user-facing Julia packages. The goal is to have
it exported to the `ortools/julia` folder in the
[GitHub repository](https://github.com/google/or-tools), but it is not yet done.

A Julia package is made up of many files, here is a short description of the
ones in each package folder:

*   `Project.toml`: description of the project and its dependencies. Update it
    when there is a new version.
    [Official documentation](https://pkgdocs.julialang.org/v1/toml-files/#Project.toml).
*   `Manifest.toml`: never commit this file.
    [Official documentation](https://pkgdocs.julialang.org/v1/toml-files/#Manifest.toml).
*   `scripts`: only in `ORToolsGenerated.jl`. Julia source code for code
    generation (Protocol Buffers and C API).
*   `src`: Julia source code for the package. `ORToolsGenerated.jl` has the
    following subfolders:
    *   `src/genproto`: generated Julia source code for the Protocol Buffers
        interface.
    *   `src/genc`: generated Julia source code for the C API.
*   `test`: automated test suite.

The `ORToolsGenerated.jl` and `ORTools.jl` packages do not include any binary
dependency; they are handled through `ORTools_jll`.

### How to sync to Google3 changes?

To update the underlying OR-Tools binary, look at
[`ORTools_jll`](https://github.com/JuliaRegistries/General/tree/master/jll/O/ORTools_jll)
and the script
[`build_tarballs.jl`](https://github.com/JuliaPackaging/Yggdrasil/blob/master/O/ORTools/build_tarballs.jl)
that build OR-Tools for Julia.

If there is an update in the C API, use `scripts/TODO(user)` to update the
generated code to call the C functions.

If there is an update in the MathOpt proto interface, use
[`ORToolsGenerated.jl/scripts/gen_pb.jl`](../ORToolsGenerated.jl/scripts/gen_pb.jl)
to update the generated Protocol Buffers code. This script is automatically
called by
[`update_package.jl`](../ORToolsGenerated.jl/scripts/update_package.jl).

## Design decisions

### Storing generated code

The Julia package manager has
[a build step](https://pkgdocs.julialang.org/v1/creating-packages/#Adding-a-build-step-to-the-package)
for all packages. However, this step is only called when first installing the
package or manually. Generating the code at the build step definitely possible,
but the generated code would not be up-to-date with the installed release of
OR-Tools, which defeats the purpose of having automatically generated code.

All other Julia packages that rely on generated code in the JuMP ecosystem rely
on stored generated code, at times storing several versions of the generated
code to accommodate various versions of the native dependency.

The package `ORToolsGenerated.jl` is supposed to correspond to exactly one
version of the native dependency.

### Role of `update_package.jl`

Bumping the version of `ORToolsGenerated.jl` requires two things:

*   Updating the version of the package
    ([following the usual rules for new releases of Julia packages](https://juliaregistries.github.io/RegistryCI.jl/stable/guidelines/#New-versions-of-existing-packages),
    which means avoiding skipping versions).
*   Updating the compatibility with `ORTools_jll`.

`update_package.jl` automates those two steps based on the version of
`ORTools_jll`.
