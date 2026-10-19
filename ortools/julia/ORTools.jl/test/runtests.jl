using ORTools
using Test

@testset "ORTools.jl" begin
    @testset "CP-SAT wrapper" begin
        include(joinpath("moi", "MOI_wrapper.jl"))
        include(joinpath("moi", "CPSat_wrapper.jl"))
        include("cli_c_tests.jl")
    end
end
