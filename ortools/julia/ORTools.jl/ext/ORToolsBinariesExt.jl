module ORToolsBinariesExt

using ORTools
using ORToolsBinaries

function __init__()
  ORTools.set_library(ORToolsBinaries.libortools)
  @info "ORTools.jl loaded ORToolsBinaries: $(ORToolsBinaries.libortools)"
end

end
