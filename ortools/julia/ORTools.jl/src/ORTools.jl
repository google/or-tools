module ORTools

import MathOptInterface as MOI

# Path to the OR-Tools shared library.
const libortools = Ref{String}("")

# Set the library path to be used by the C wrapper. This function is mostly
# intended for use by the extension packages.
#
# If you want to use a custom library, call this function with the path to the
# library before using any other function in this package (technically, any
# function that uses the C APIs like `solve`).
function set_library(path::String)
  libortools[] = path
  return
end

function __init__()
  if Base.find_package("ORTools_jll") === nothing &&
     Base.find_package("ORToolsBinaries") === nothing
    # TODO(user): update when we support remote solves, it should still be
    # a warning, but the package will work (albeit only locally).
    @warn "Neither ORTools_jll nor ORToolsBinaries is installed. Install one " *
          "of them to use OR-Tools. ORTools.jl will not work without either. " *
          "Import one of these two packages."
    # For the reader: when the two are installed and imported, the one that gets
    # used is the last one.
  end
end

# Lower level C-dependency.
include("c_wrapper/utils.jl")
include("c_wrapper/cpsat.jl")
include("c_wrapper/math_opt.jl")
# MOI-level wrappers.
include("moi_wrapper/utils.jl")
include("moi_wrapper/Type_wrappers.jl")
include("moi_wrapper/MOI_wrapper.jl")

end
