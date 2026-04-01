module ORToolsJllExt

using ORTools
using ORTools_jll

function __init__()
  ORTools.set_library(ORTools_jll.libortools)
  @info "ORTools.jl loaded ORTools_jll: $(ORTools_jll.libortools)"
end

end
