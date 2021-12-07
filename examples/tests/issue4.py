#!/usr/bin/env python3
from ortools.constraint_solver import pywrapcp


def main():
  solver = pywrapcp.Solver("time limit test")
  n = 10
  x = [solver.IntVar(1, n, "x[%i]" % i) for i in range(n)]
  solver.Add(solver.AllDifferent(x, True))

  solution = solver.Assignment()
  solution.Add(x)

  db = solver.Phase(x,
                    solver.CHOOSE_FIRST_UNBOUND,
                    solver.ASSIGN_MIN_VALUE)

  time_limit = 2000
  branch_limit = 100000000
  failures_limit = 100000000
  solutions_limit = 10000000
  limits = (
      solver.Limit(
          time_limit, branch_limit, failures_limit, solutions_limit, True))

  search_log = solver.SearchLog(1000)

  solver.NewSearch(db, [limits, search_log])
  num_solutions = 0
  while solver.NextSolution():
    print("x:", [x[i].Value() for i in range(n)])
    num_solutions += 1
  solver.EndSearch()

  print("num_solutions:", num_solutions)
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("wall_time:", solver.WallTime())


if __name__ == "__main__":
  main()
