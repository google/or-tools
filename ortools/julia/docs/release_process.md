# Julia Release process

This document is meant to collect all knowledge related to releasing the Julia
packages for OR-Tools.

The current architecture uses four distinct packages:

*   `ORTools_jll.jl`: provides the OR-Tools binaries, built using the native
    Julia infrastructure; this is the preferred way for users to install
    OR-Tools binaries, but we cannot use it for all platforms we care about.
    This package is hosted within JuliaPackaging infrastructure.
*   `ORToolsBinaries.jl`: provides the OR-Tools binaries, downloaded from
    GitHub. This package acts as a fallback whenever `ORTools_jll.jl` is not
    available.
*   `ORToolsGenerated.jl`: provides the generated code from OR-Tools, which
    currently consists of ProtoBuf files.
*   `ORTools.jl`: provides access to OR-Tools features from Julia. In
    particular, it integrates OR-Tools with MathOptInterface and thus JuMP.

[TOC]

## Updating the packages upon an OR-Tools release

The major steps are:

1.  Update
    [the Yggdrasil build script](https://github.com/JuliaPackaging/Yggdrasil/blob/master/O/ORTools/build_tarballs.jl)
    for building the native binary on
    [JuliaPackaging/Yggdrasil](https://github.com/JuliaPackaging/Yggdrasil).
    Making a PR will automatically generate the updated the binary package
    [ORTools_jll.jl](https://github.com/JuliaBinaryWrappers/ORTools_jll.jl).

note: we are generating our Julia project from a CMake based build system.

2.  Update the paths in `ORToolsBinaries.jl` to point to the new packages on
    GitHub.
3.  If needed (when the ProtoBuf files change), regenerate the Julia package
    with generated code for OR-Tools, `ORToolsGenerated.jl`, and publish it.
4.  `ORTools.jl` should usually not be updated at the same time as binaries.

## Building binaries

To publish a new release of OR-Tools binary Julia package (which will be
available using `import ORTools_jll`), you have to update the Yggdrasil script
that builds the library from scratch in a virtual environment defined by
[BinaryBuilder.jl](https://docs.binarybuilder.org/stable/).

The virtual environment is based on Linux and offers cross-compilation toolchain
for each platform to support. These toolchains can slightly be modified by hand,
for instance when updating the version of CMake. The toolchains are not entirely
standard, for instance the parameter `-march` is forbidden for the compiler (its
value is supposed to be determined from the cross-compilation toolchain).

Since OR-Tools is written in C++, our Julia packages must be built on the
supported OSes (as of June 2025, Linux and FreeBSD) and the only supported
architecture (`x86-64`).

note: The binary package `ORTools_jll.jl` must offer packages built with
`BinaryBuilder.jl`. Yggdrasil's maintainers will not accept uploading our GitHub
packages. These GitHub packages are offered through `ORToolsBinaries.jl`.

## Julia release process

### Update the `ORTools_jll.jl` package

The process is mostly automated: a small PR is required to start the process and
no other interaction should be required (unless the build script needs
updating).

Fork the [Yggdrasil GitHub repo](https://github.com/JuliaPackaging/Yggdrasil)
and edit the Yggdrasil build script for
[OR-Tools](https://github.com/JuliaPackaging/Yggdrasil/blob/master/O/ORTools/build_tarballs.jl):

1.  Update the package version (the `version` variable). It should match
    OR-Tools' version.
2.  Update the Git SHA-1 to the corresponding commit in
    [OR-Tools' repo](https://github.com/google/or-tools) (the `sources`
    variable).

Make the PR. Automatically, GitHub starts building packages (one runner per
target platform). You can have a look at the build logs as for any GitHub check.

note: The PR can only be merged if building succeeds for all platforms! If you
can fix the error, do so in the same PR; otherwise, disable the problematic
platform(s).

### Update the `ORToolsBinaries.jl` package

For platforms where the above technique does not work (such as Windows or Arm),
we have `ORToolsBinaries.jl`. The process is not automated at all.

In `third_party/ortools/ortools/julia/ORToolsBinaries.jl/deps/build.jl`,
update the first few lines when releasing a new version:

*   `ORTOOLS_MINOR_VERSION` and `ORTOOLS_PATCH_VERSION`: version of OR-Tools to
    be downloaded.
*   `PACKAGE_FILE_NAME_WITHOUT_EXTENSION`: structure for the filenames of the
    binary packages on GitHub. They should only change once in a while with
    updates to the build process, such as a new distribution.

To test the changes, you can run the lines 6 to 20 in a Julia command prompt and
inspect the `TARGET_PACKAGES` dictionary: it contains the URLs of all the binary
packages that can be downloaded from `ORToolsBinaries.jl`.

Once the changes are on GitHub, you can register the new version of the package
(after updating ORToolsBinaries.jl' `Project.toml`, see below) with the
following comment on the Git commit with the latest changes:

```
@JuliaRegistrator register subdir=ortools/julia/ORToolsBinaries.jl
```

The Julia code on GitHub can contain references to versions that are yet to be
released; the new version cannot be registered unless the version of OR-Tools is
public (i.e. the URLs point at the right packages). In particular, this means
that, once you know the URLs for the packages, you can update the code for the
package, then tag the new release for Julia at the same time as the rest of
package managers.

### Update the `ORToolsGenerated.jl` package

`ORToolsGenerated.jl`, the Julia package containing generated code, should be
rebuilt when protos are modified and must be when the new features are used in
Julia code. It is therefore expected that the package is seldom updated once
every few years.

To do so:

1.  Run the code generator, equivalent to `protoc` (command relative to
    Google3):

```sh
juliaortools/julia/ORToolsGenerated.jl/scripts/gen_pb.jl
```

2.  Update the package metadata
    [`Project.toml`](../ORToolsGenerated.jl/Project.toml)
    to bump its version (only update the `version` field). Use good judgment and
    try to follow [SemVer](https://semver.org/) when picking a new version.
3.  Upload the updated code to GitHub in the
    [`ORToolsGenerated.jl`](https://github.com/google/or-tools/tree/stable/ortools/julia/ORToolsGenerated.jl)
    folder by making a Git commit.
4.  On GitHub, navigate to the Git commit you just made.
5.  Register the new package by adding a comment to the Git commit on GitHub:

```
@JuliaRegistrator register subdir=ortools/julia/ORToolsGenerated.jl
```

For instance,
[or-tools#e9ccb8e](https://github.com/google/or-tools/commit/e9ccb8e73bd4aeb688fb2a62d7f4918607d74ad6)
registered the first version of the package. All updates should follow the same
process.

[JuliaRegistrator](https://github.com/JuliaRegistries/Registrator.jl) is a bot
that handles the registration operation automatically based on the metadata in
`Project.toml`. It will post a comment indicating whether there are problems
that prevent package registration or a link to the PR that updates the Julia
package registry. Package updates are typically automatically merged.

### Update the `ORTools.jl` package

The most interesting part of the Julia code is in the `ORTools.jl` package. In
particular, it contains the
[MathOptInterface.jl](https://github.com/jump-dev/MathOptInterface.jl) wrapper
for MathOpt.

The release process is similar to that of `ORToolsGenerated.jl`, with few
exceptions: there is no code to generate; updates should only happen when there
is a change to the Julia code (not for every OR-Tools release, apart from
technically breaking releases of toher packages).

To register a new version of the package:

```
@JuliaRegistrator register subdir=ortools/julia/ORTools.jl
```

## Miscellaneous

### Check runtime package

The Julia wrapper requires a few symbols to be available in the binary package.
To check their presence manually, run this command (relative to Google3):

```sh
juliaortools/julia/ORTools.jl/test/cli_c_tests.jl /path/to/libortools.so.dll
```

This script attempts loading the dynamic library and checks whether the expected
symbols can be loaded.

## FAQ

### The published package builds but doesn't work when installed; how do I fix it?

You can publish several versions of `ORTools_jll.jl` for one version of
OR-Tools, like `9.13.0-1` for a small fix on top of OR-Tools 9.13.0.

### I need to apply a specific patch to OR-Tools for Julia that cannot be applied to the main repository. What do I do?

You can add patches to the Yggdrasil build script. Add the patches in the
`bundled/patches` folder. Ensure the folder is in the `sources` variable (along
with the Git repository) so that the patches are available at build time:

```
sources = [
    GitSource("https://github.com/google/or-tools.git", "..."),
    DirectorySource("./bundled")
]
```

Update the Yggdrasil build script in the `script` variable to apply the patches:

```
# Prepare the source directory.Add commentMore actions
cd $WORKSPACE/srcdir/or-tools*
atomic_patch -p1 "${WORKSPACE}/srcdir/patches/some.patch"
mkdir build
```

See
[Yggdrasil#7684](https://github.com/JuliaPackaging/Yggdrasil/commit/51fcf9e74a0a5b077cd5e03fb9edab9b0618b0af)
for a live example.

In general, though, you can use any commit for a binary Julia release: you can
create a specific branch for one version or use a development branch.

### I would like to support more platforms, how do I enable them?

Uncomment the platform in the `platforms` variable of the Yggdrasil build
script: the GitHub runners will attempt building the package on all the
platforms in this variable.

### How do I update (build) dependencies?

You can add build dependencies in the `dependencies` variable of the
Yggdrasil build script. A `HostBuildDependency` is only used at build time,
while a `Dependency` is used at run time. If you need to update the compiler
version, you can update the version in the call to `build_tarballs` in the
Yggdrasil build script, but you can only indicate preferences; updating the
minimum version of Julia is a surer shot.
