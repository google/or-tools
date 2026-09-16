import MathOptInterface as MOI
include("../src/ORTools.jl")

"""
  Solving an AllDifferent problem using the CP-SAT solver.
"""

function main(ARGS)
    optimizer = ORTools.CPSATOptimizer()

    x = MOI.VectorOfVariables([
        MOI.add_constrained_variable(optimizer, MOI.Interval(1, 5)) for _ = 1:5
    ])

    MOI.add_constraint(optimizer, x, MOI.AllDifferent(5))

    MOI.optimize!(optimizer)
end

main(ARGS)
