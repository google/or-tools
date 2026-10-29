import MathOptInterface as MOI
import ORTools

"""
Solve the following optimization problem:

  max x + 2⋅y

  subject to: x + y ≤ 1.5
              -1 ≤ x ≤ 1.5
              0 ≤ y ≤ 1
"""

function main(ARGS)
    optimizer = ORTools.Optimizer(
        solver_type = ORTools.SolverType.SOLVER_TYPE_GLOP
    )

    # Define the variables: x and y
    vars = MOI.add_variables(optimizer, 2)

    # Set the objective: maximize x + 2y
    MOI.set(
        optimizer,
        MOI.ObjectiveFunction{MOI.ScalarAffineFunction{Float64}}(),
        MOI.ScalarAffineFunction(MOI.ScalarAffineTerm.([1.0, 2.0], vars), 0.0),
    )

    MOI.set(optimizer, MOI.ObjectiveSense(), MOI.MAX_SENSE)

    # The first constraint: x + y <= 1.5
    C1 = 1.5
    MOI.add_constraint(
        optimizer,
        MOI.ScalarAffineFunction(
            [MOI.ScalarAffineTerm(1.0, vars[1]),
             MOI.ScalarAffineTerm(1.0, vars[2])],
            0.0,
        ),
        MOI.LessThan(C1),
    )

    # The second constraint: -1 <= x <= 1.5
    MOI.add_constraint(optimizer, MOI.VariableIndex(1), MOI.Interval(-1.0, 1.5))

    # The third constraint: 0 <= y <= 1
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
