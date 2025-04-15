# TODO: b/407034730 - remove this file.
import MathOptInterface as MOI
include("ORTools.jl")

"""
Solving the following optimization problem:

  max x + 2⋅y
  
  subject to: x + y ≤ 1.5
              -1 ≤ x ≤ 1.5
              0 ≤ y ≤ 1
"""

function main(ARGS)
    optimizer = ORTools.Optimizer()
    c = [1.0, 2.0]

    # Variables definition
    x = MOI.add_variables(optimizer, length(c))

    # Set the objective
    MOI.set(
        optimizer,
        MOI.ObjectiveFunction{MOI.ScalarAffineFunction{Float64}}(),
        MOI.ScalarAffineFunction(MOI.ScalarAffineTerm.(c, x), 0.0),
    )

    MOI.set(optimizer, MOI.ObjectiveSense(), MOI.MAX_SENSE)

    # The first constraint (x + y <= 1.5)
    C1 = 1.5
    MOI.add_constraint(
        optimizer,
        MOI.ScalarAffineFunction(
            [MOI.ScalarAffineTerm(1.0, x[1]), MOI.ScalarAffineTerm(1.0, x[2])],
            0.0,
        ),
        MOI.LessThan(C1),
    )

    ## The second constraint (-1 <= x <= 1.5)
    MOI.add_constraint(optimizer, MOI.VariableIndex(1), MOI.Interval(-1.0, 1.5))

    ## The third constraint (0 <= y <= 1)
    MOI.add_constraint(optimizer, MOI.VariableIndex(2), MOI.Interval(0.0, 1.0))

    print(optimizer)

    # Optimize the model
    MOI.optimize!(optimizer)

    # Print the solution
    print(optimizer.solve_result)

    return 0
end

# TODO: Understand why the solver stopped.
# TODO: Understand what solution was returned.
# TODO: Query the objective.
# TODO: Query the primal solution.
main(ARGS)
