using ORTools
using Test

@testset "ORTools.jl" begin
    for file in readdir("moi")
        include(joinpath("moi", file))
    end
end
