from ortools.constraint_solver import pywrapcp


def main():
  # Create the solver.
  solver = pywrapcp.Solver('Vendors scheduling')

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
  possible_schedules = [[1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 8],
                        [1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 4],
                        [0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 2, 5],
                        [0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 3, 4],
                        [1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 4, 3],
                        [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0]]

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
      x[i, j] = solver.IntVar(0, num_work_types, 'x[%i,%i]' % (i, j))
      tmp.append(x[i, j])
    selected_schedule = solver.IntVar(0, num_possible_schedules - 1,
                                      's[%i]' % i)
    hours = solver.IntVar(0, num_hours, 'h[%i]' % i)
    selected_schedules.append(selected_schedule)
    vendors_stat.append(hours)
    tmp.append(selected_schedule)
    tmp.append(hours)

    solver.Add(solver.AllowedAssignments(tmp, possible_schedules))

  #
  # Statistics and constraints for each hour
  #
  for j in range(num_hours):
    workers = solver.Sum([x[i, j] for i in range(num_vendors)]).Var()
    hours_stat.append(workers)
    solver.Add(workers * max_trafic_per_vendor >= trafic[j])

  #
  # Redundant constraint: sort selected_schedules
  #
  for i in range(num_vendors - 1):
    solver.Add(selected_schedules[i] <= selected_schedules[i + 1])

  #
  # Search
  #
  db = solver.Phase(selected_schedules, solver.CHOOSE_FIRST_UNBOUND,
                    solver.ASSIGN_MIN_VALUE)

  solver.NewSearch(db)

  num_solutions = 0
  while solver.NextSolution():
    num_solutions += 1

    for i in range(num_vendors):
      print('Vendor %i: ' % i,
            possible_schedules[selected_schedules[i].Value()])
    print()

    print('Statistics per day:')
    for j in range(num_hours):
      print('Day%2i: ' % j, end=' ')
      print(hours_stat[j].Value(), end=' ')
      print()
    print()

  solver.EndSearch()
  print()
  print('num_solutions:', num_solutions)
  print('failures:', solver.Failures())
  print('branches:', solver.Branches())
  print('WallTime:', solver.WallTime(), 'ms')


if __name__ == '__main__':
  main()
