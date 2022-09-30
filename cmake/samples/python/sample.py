"""Sample to test or-tools installation."""
import ortools
# from ortools.algorithms import pywrapknapsack_solver
from ortools.constraint_solver import pywrapcp
# from ortools.graph.python import linear_sum_assignment
# from ortools.graph.python import max_flow
# from ortools.graph.python import min_cost_flow
from ortools.linear_solver import pywraplp
# from ortools.linear_solver import linear_solver_pb2
# from ortools.sat.python import swig_helper
# from ortools.sat.python import cp_model
# from ortools.scheduling import pywraprcpsp
# from ortools.util.python import sorted_interval_list


def lpsolver_test():
  """Test pywraplp."""
  print('Test lpsolver...')
  lpsolver = pywraplp.Solver('LinearTest',
                             pywraplp.Solver.GLOP_LINEAR_PROGRAMMING)
  lpsolver.Solve()
  print('Test lpsolver...DONE')


def cpsolver_test():
  """Test pywrapcp."""
  print('Test cpsolver...')
  cpsolver = pywrapcp.Solver('ConstraintTest')
  num_vals = 3
  x = cpsolver.IntVar(0, num_vals - 1, 'x')
  y = cpsolver.IntVar(0, num_vals - 1, 'y')
  z = cpsolver.IntVar(0, num_vals - 1, 'z')
  cpsolver.Add(x != y)
  db = cpsolver.Phase([x, y, z], cpsolver.CHOOSE_FIRST_UNBOUND,
                      cpsolver.ASSIGN_MIN_VALUE)
  cpsolver.Solve(db)
  print('Test cpsolver...DONE')


def main():
  print(ortools.__version__)
  lpsolver_test()
  cpsolver_test()


if __name__ == '__main__':
  main()
