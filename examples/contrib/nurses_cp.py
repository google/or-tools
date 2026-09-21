import sys

from ortools.constraint_solver.python import constraint_solver as cp


def main():
    # Creates the solver.
    solver = cp.Solver("schedule_shifts")

    num_nurses = 4
    num_shifts = 4  # Nurse assigned to shift 0 means not working that day.
    num_days = 7
    # [START]
    # Create shift variables.
    shifts = {}

    for j in range(num_nurses):
        for i in range(num_days):
            shifts[(j, i)] = solver.new_int_var(
                0, num_shifts - 1, "shifts(%i,%i)" % (j, i)
            )
    shifts_flat = [shifts[(j, i)] for j in range(num_nurses) for i in range(num_days)]

    # Create nurse variables.
    nurses = {}

    for j in range(num_shifts):
        for i in range(num_days):
            nurses[(j, i)] = solver.new_int_var(
                0, num_nurses - 1, "shift%d day%d" % (j, i)
            )
    # Set relationships between shifts and nurses.
    for day in range(num_days):
        nurses_for_day = [nurses[(j, day)] for j in range(num_shifts)]

        for j in range(num_nurses):
            s = shifts[(j, day)]
            solver.add(s.index_of(nurses_for_day) == j)
    # Make assignments different on each day
    for i in range(num_days):
        solver.add_all_different([shifts[(j, i)] for j in range(num_nurses)])
        solver.add_all_different([nurses[(j, i)] for j in range(num_shifts)])
    # Each nurse works 5 or 6 days in a week.
    for j in range(num_nurses):
        solver.add(solver.sum([shifts[(j, i)] > 0 for i in range(num_days)]) >= 5)
        solver.add(solver.sum([shifts[(j, i)] > 0 for i in range(num_days)]) <= 6)
    # Create works_shift variables. works_shift[(i, j)] is True if nurse
    # i works shift j at least once during the week.
    works_shift = {}

    for i in range(num_nurses):
        for j in range(num_shifts):
            works_shift[(i, j)] = solver.new_bool_var("nurse%d shift%d" % (i, j))

    for i in range(num_nurses):
        for j in range(num_shifts):
            solver.add(
                works_shift[(i, j)]
                == solver.max([shifts[(i, k)] == j for k in range(num_days)])
            )

    # For each shift (other than 0), at most 2 nurses are assigned to that shift
    # during the week.
    for j in range(1, num_shifts):
        solver.add(solver.sum([works_shift[(i, j)] for i in range(num_nurses)]) <= 2)
    # If s nurses works shifts 2 or 3 on, he must also work that shift the previous
    # day or the following day.
    solver.add(
        solver.max(nurses[(2, 0)] == nurses[(2, 1)], nurses[(2, 1)] == nurses[(2, 2)])
        == 1
    )
    solver.add(
        solver.max(nurses[(2, 1)] == nurses[(2, 2)], nurses[(2, 2)] == nurses[(2, 3)])
        == 1
    )
    solver.add(
        solver.max(nurses[(2, 2)] == nurses[(2, 3)], nurses[(2, 3)] == nurses[(2, 4)])
        == 1
    )
    solver.add(
        solver.max(nurses[(2, 3)] == nurses[(2, 4)], nurses[(2, 4)] == nurses[(2, 5)])
        == 1
    )
    solver.add(
        solver.max(nurses[(2, 4)] == nurses[(2, 5)], nurses[(2, 5)] == nurses[(2, 6)])
        == 1
    )
    solver.add(
        solver.max(nurses[(2, 5)] == nurses[(2, 6)], nurses[(2, 6)] == nurses[(2, 0)])
        == 1
    )
    solver.add(
        solver.max(nurses[(2, 6)] == nurses[(2, 0)], nurses[(2, 0)] == nurses[(2, 1)])
        == 1
    )

    solver.add(
        solver.max(nurses[(3, 0)] == nurses[(3, 1)], nurses[(3, 1)] == nurses[(3, 2)])
        == 1
    )
    solver.add(
        solver.max(nurses[(3, 1)] == nurses[(3, 2)], nurses[(3, 2)] == nurses[(3, 3)])
        == 1
    )
    solver.add(
        solver.max(nurses[(3, 2)] == nurses[(3, 3)], nurses[(3, 3)] == nurses[(3, 4)])
        == 1
    )
    solver.add(
        solver.max(nurses[(3, 3)] == nurses[(3, 4)], nurses[(3, 4)] == nurses[(3, 5)])
        == 1
    )
    solver.add(
        solver.max(nurses[(3, 4)] == nurses[(3, 5)], nurses[(3, 5)] == nurses[(3, 6)])
        == 1
    )
    solver.add(
        solver.max(nurses[(3, 5)] == nurses[(3, 6)], nurses[(3, 6)] == nurses[(3, 0)])
        == 1
    )
    solver.add(
        solver.max(nurses[(3, 6)] == nurses[(3, 0)], nurses[(3, 0)] == nurses[(3, 1)])
        == 1
    )
    # Create the decision builder.
    db = solver.phase(
        shifts_flat,
        cp.IntVarStrategy.CHOOSE_FIRST_UNBOUND,
        cp.IntValueStrategy.ASSIGN_MIN_VALUE,
    )
    # Create the solution collector.
    solution = solver.assignment()
    solution.add(shifts_flat)
    collector = solver.all_solution_collector(solution)

    solver.solve(db, [collector])
    print("Solutions found:", collector.solution_count)
    print("Time:", solver.wall_time_ms, "ms")
    print()
    # Display a few solutions picked at random.
    a_few_solutions = [859, 2034, 5091, 7003]

    for sol in a_few_solutions:
        print("Solution number", sol, "\n")

        for i in range(num_days):
            print("Day", i)
            for j in range(num_nurses):
                print(
                    "Nurse", j, "assigned to task", collector.value(sol, shifts[(j, i)])
                )
            print()


if __name__ == "__main__":
    main()
