# Run this script as:
#     hgd / g4d ...
#     cd third_party/ortools/ortools/julia/ORToolsGenerated.jl/scripts/
#     julia gen_pb.jl

using ORTools_jll
using ProtoBuf
using Pkg

# Check the version of ProtoBuf.jl.
# This code does not use `ProtoBuf.PACKAGE_VERSION`, as it is as ad-hoc as this
# code and is not exported from the package.
protobuf_jl_version = [dep.version for (uuid, dep) in Pkg.dependencies() if dep.name == "ProtoBuf"][1]
if protobuf_jl_version < v"1.0.15"
    # MathOpt requires https://github.com/JuliaIO/ProtoBuf.jl/pull/234.
    # Bop requires https://github.com/JuliaIO/ProtoBuf.jl/pull/244.
    error("ProtoBuf.jl has a too low version: 1.0.15 is the minimum required")
    # Version 1.1 also works.
end

# Path to the generated Julia package.
PACKAGE_ROOT = abspath(joinpath(dirname(@__FILE__), ".."))
# Path to the exact folder where the generated files go.
OUTPUT_PATH = joinpath(PACKAGE_ROOT, "src", "genproto")
# Root of the ProtoBuf definitions, within ORTools_jll.
PROTO_ROOT_PATH = begin
    path = ORTools_jll.get_proto_linear_solver_path()
    path[1:length(path) - 41]
end
@assert endswith(PROTO_ROOT_PATH, "/")

# If you want to list the files in `PROTO_ROOT_PATH` instead of relying on the
# JLL package:
# proto_abs_paths = collect(Iterators.flatten([[joinpath(root, file) for file in files if endswith(file, ".proto") && occursin("ortools", root)] for (root, dirs, files) in walkdir(PROTO_ROOT_PATH)]))

proto_var_names = [file for file in names(ORTools_jll) if startswith(String(file), "proto_")]
proto_abs_paths = eval.(Meta.parse.("ORTools_jll." .* String.(proto_var_names)))
proto_rel_paths = replace.(proto_abs_paths, PROTO_ROOT_PATH => "")

println("List of the $(length(proto_var_names)) protos being included:")
println(proto_var_names)

mkpath(OUTPUT_PATH)
protojl(proto_rel_paths, PROTO_ROOT_PATH, OUTPUT_PATH)
