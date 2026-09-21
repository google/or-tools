from ortools.constraint_solver.python import constraint_solver as cp


def main():
    # Create the solver.
    solver = cp.Solver("Vendors scheduling")

    #
    # data
    #
    num_vendors = 9
    num_hours = 10
    num_work_types = 1

    trafic = [100, 500, 100, 200, 320, 300, 200, 220, 300, 120]
    max_trafic_per_vendor = 100

    # Last columns are :
    #   index_of_the_schedule, sum of worked hours (per work type).
    # The index is useful for branching.
    possible_schedules = [
        [1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 8],
        [1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 4],
        [0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 2, 5],
        [0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 3, 4],
        [1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 4, 3],
        [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0],
    ]

    num_possible_schedules = len(possible_schedules)
    selected_schedules = []
    vendors_stat = []
    hours_stat = []

    #
    # declare variables
    #
    x = {}

    for i in range(num_vendors):
        tmp = []
        for j in range(num_hours):
            x[i, j] = solver.new_int_var(0, num_work_types, "x[%i,%i]" % (i, j))
            tmp.append(x[i, j])
        selected_schedule = solver.new_int_var(
            0, num_possible_schedules - 1, "s[%i]" % i
        )
        hours = solver.new_int_var(0, num_hours, "h[%i]" % i)
        selected_schedules.append(selected_schedule)
        vendors_stat.append(hours)
        tmp.append(selected_schedule)
        tmp.append(hours)

        solver.add_allowed_assignments(tmp, possible_schedules)

    #
    # Statistics and constraints for each hour
    #
    for j in range(num_hours):
        workers = solver.sum([x[i, j] for i in range(num_vendors)]).var()
        hours_stat.append(workers)
        solver.add(workers * max_trafic_per_vendor >= trafic[j])

    #
    # Redundant constraint: sort selected_schedules
    #
    for i in range(num_vendors - 1):
        solver.add(selected_schedules[i] <= selected_schedules[i + 1])

    #
    # Search
    #
    db = solver.phase(
        selected_schedules,
        cp.IntVarStrategy.CHOOSE_FIRST_UNBOUND,
        cp.IntValueStrategy.ASSIGN_MIN_VALUE,
    )

    solver.new_search(db)

    num_solutions = 0
    while solver.next_solution():
        num_solutions += 1

        for i in range(num_vendors):
            print("Vendor %i: " % i, possible_schedules[selected_schedules[i].value()])
        print()

        print("Statistics per day:")
        for j in range(num_hours):
            print("Day%2i: " % j, end=" ")
            print(hours_stat[j].value(), end=" ")
            print()
        print()

    solver.end_search()
    print()
    print("num_solutions:", num_solutions)
    print("failures:", solver.num_failures)
    print("branches:", solver.num_branches)
    print("WallTime:", solver.wall_time_ms, "ms")


if __name__ == "__main__":
    main()
