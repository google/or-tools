using ORTools_jll
using ProtoBuf
using Pkg

ortools_generated_jl_version = [dep.version for (uuid, dep) in Pkg.dependencies() if dep.name == "ORToolsGenerated"]
if length(ortools_generated_jl_version) > 0
    error("To run the update script for ORToolsGenerated, you cannot have it installed. You currently have version $(ortools_generated_jl_version) of ORToolsGenerated.jl.")
end

# Path to the generated Julia package.
PACKAGE_ROOT = abspath(joinpath(dirname(@__FILE__), ".."))
if !endswith(PACKAGE_ROOT, "ORToolsGenerated.jl") && !endswith(PACKAGE_ROOT, "ORToolsGenerated.jl/")
    error("The path to the ORToolsGenerated package was expected to end with ORToolsGenerated.jl, found $(PACKAGE_ROOT) instead.")
end
if endswith(PACKAGE_ROOT, "ORToolsGenerated.jl/")
    PACKAGE_ROOT = PACKAGE_ROOT[begin:end-1]
end

# Get the current ORTools_jll version.
ortools_jll_version = [dep.version for (uuid, dep) in Pkg.dependencies() if dep.name == "ORTools_jll"]
if length(ortools_jll_version) == 0
    error("To run the update script for ORToolsGenerated, you must have ORTools_jll installed.")
end
ortools_jll_version = ortools_jll_version[1]

# Update Project.toml.
project_toml_path = PACKAGE_ROOT * "/Project.toml"
project_toml = read(project_toml_path, String)

previous_version = VersionNumber(match(r"version = \"(.*)\"", project_toml)[1])
next_version = VersionNumber(previous_version.major, previous_version.minor, previous_version.patch + 1, previous_version.prerelease, previous_version.build)
project_toml = replace(project_toml, "version = \"$(previous_version)\"" => "version = \"$(next_version)\"")

project_toml = replace(project_toml, r"ORTools_jll = \"(.*)\"" => "ORTools_jll = \"$(ortools_jll_version)\"")

write(project_toml_path, project_toml)

# Regenerate the ProtoBuf files.
if isdir("$(PACKAGE_ROOT)/src/genproto")
    Base.Filesystem.rm("$(PACKAGE_ROOT)/src/genproto", recursive = true)
end
include("gen_pb.jl")
