# ORToolsGenerated

This package only contains generated code for use by `ORTools.jl`.

For now, there is a single type of generated files: those from
[ProtoBuf.jl](https://github.com/JuliaIO/ProtoBuf.jl), the Julia compiler for
[Protocol Buffers](https://protobuf.dev/).

## For users

You probably do not need to use this package directly in your application. The
MathOptInterface wrapper in `ORTools.jl` should provide you with all the
required features. This package is likely to be installed as a dependency of
`ORTools.jl`

## For maintainers

Human and agent-authored commits in `src` are discouraged, apart from the entry
point `ORToolsGenerated.jl` to include the relevant generated files (which
should not change regularly if at all).

To regenerate the files in `src/genproto`, use the script in
`scripts/run_gen_pb_docker.sh`. Technically, it will use Docker or Podman with a
predefined LTS version of Julia to run `ProtoBuf.jl` and update all the
generated files. You can then proceed with making a new release of
`ORToolsGenerated.jl`.

The script requires a single argument, the path to the `ORToolsGenerated.jl`
package (where the output is written). For instance, if running from the
`scripts` folder, use:

```
bash run_gen_pb_docker.sh ../
```

**Warning:** This script uses the proto files from the installed version of
`ORTools_jll`. You must therefore update the JLL package before running the
script.
