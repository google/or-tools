module ORTools

import MathOptInterface as MOI
using ORTools_jll

# Lower level C-dependency.
include("c_wrapper/c_wrapper.jl")
# MOI Wrappers and related utils.
include("moi_wrapper/utils.jl")
include("moi_wrapper/Type_wrappers.jl")
include("moi_wrapper/MOI_wrapper.jl")

end
