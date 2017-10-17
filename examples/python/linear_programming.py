# Copyright 2010-2017 Google
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Linear programming examples that show how to use the APIs."""
from __future__ import print_function
from ortools.linear_solver import linear_solver_pb2
from ortools.linear_solver import pywraplp


def RunLinearExampleNaturalLanguageAPI(optimization_problem_type):
  """Example of simple linear program with natural language API."""
  solver = pywraplp.Solver('RunLinearExampleNaturalLanguageAPI',
                           optimization_problem_type)
  infinity = solver.infinity()
  # x1, x2 and x3 are continuous non-negative variables.
  x1 = solver.NumVar(0.0, infinity, 'x1')
  x2 = solver.NumVar(0.0, infinity, 'x2')
  x3 = solver.NumVar(0.0, infinity, 'x3')

  solver.Maximize(10 * x1 + 6 * x2 + 4 * x3)
  c0 = solver.Add(10 * x1 + 4 * x2 + 5 * x3 <= 600, 'ConstraintName0')
  c1 = solver.Add(2 * x1 + 2 * x2 + 6 * x3 <= 300)
  sum_of_vars = sum([x1, x2, x3])
  c2 = solver.Add(sum_of_vars <= 100.0, 'OtherConstraintName')

  SolveAndPrint(solver, [x1, x2, x3], [c0, c1, c2])
  # Print a linear expression's solution value.
  print(('Sum of vars: %s = %s' % (sum_of_vars, sum_of_vars.solution_value())))


def RunLinearExampleCppStyleAPI(optimization_problem_type):
  """Example of simple linear program with the C++ style API."""
  solver = pywraplp.Solver('RunLinearExampleCppStyle',
                           optimization_problem_type)
  infinity = solver.infinity()
  # x1, x2 and x3 are continuous non-negative variables.
  x1 = solver.NumVar(0.0, infinity, 'x1')
  x2 = solver.NumVar(0.0, infinity, 'x2')
  x3 = solver.NumVar(0.0, infinity, 'x3')

  # Maximize 10 * x1 + 6 * x2 + 4 * x3.
  objective = solver.Objective()
  objective.SetCoefficient(x1, 10)
  objective.SetCoefficient(x2, 6)
  objective.SetCoefficient(x3, 4)
  objective.SetMaximization()

  # x1 + x2 + x3 <= 100.
  c0 = solver.Constraint(-infinity, 100.0, 'c0')
  c0.SetCoefficient(x1, 1)
  c0.SetCoefficient(x2, 1)
  c0.SetCoefficient(x3, 1)

  # 10 * x1 + 4 * x2 + 5 * x3 <= 600.
  c1 = solver.Constraint(-infinity, 600.0, 'c1')
  c1.SetCoefficient(x1, 10)
  c1.SetCoefficient(x2, 4)
  c1.SetCoefficient(x3, 5)

  # 2 * x1 + 2 * x2 + 6 * x3 <= 300.
  c2 = solver.Constraint(-infinity, 300.0, 'c2')
  c2.SetCoefficient(x1, 2)
  c2.SetCoefficient(x2, 2)
  c2.SetCoefficient(x3, 6)

  SolveAndPrint(solver, [x1, x2, x3], [c0, c1, c2])


def SolveAndPrint(solver, variable_list, constraint_list):
  """Solve the problem and print the solution."""
  print(('Number of variables = %d' % solver.NumVariables()))
  print(('Number of constraints = %d' % solver.NumConstraints()))

  result_status = solver.Solve()

  # The problem has an optimal solution.
  assert result_status == pywraplp.Solver.OPTIMAL

  # The solution looks legit (when using solvers others than
  # GLOP_LINEAR_PROGRAMMING, verifying the solution is highly recommended!).
  assert solver.VerifySolution(1e-7, True)

  print(('Problem solved in %f milliseconds' % solver.wall_time()))

  # The objective value of the solution.
  print(('Optimal objective value = %f' % solver.Objective().Value()))

  # The value of each variable in the solution.
  for variable in variable_list:
    print(('%s = %f' % (variable.name(), variable.solution_value())))

  print('Advanced usage:')
  print(('Problem solved in %d iterations' % solver.iterations()))
  for variable in variable_list:
    print(('%s: reduced cost = %f' % (variable.name(), variable.reduced_cost())))
  activities = solver.ComputeConstraintActivities()
  for i, constraint in enumerate(constraint_list):
    print(('constraint %d: dual value = %f\n'
          '               activity = %f' %
          (i, constraint.dual_value(), activities[constraint.index()])))


def main():
  all_names_and_problem_types = (
      list(linear_solver_pb2.MPModelRequest.SolverType.items()))
  for name, problem_type in all_names_and_problem_types:
    # Skip non-LP problem types.
    if not name.endswith('LINEAR_PROGRAMMING'):
      continue
    # Skip problem types that aren't supported by the current binary.
    if not pywraplp.Solver.SupportsProblemType(problem_type):
      continue
    print(('\n------ Linear programming example with %s ------' % name))
    print('\n*** Natural language API ***')
    RunLinearExampleNaturalLanguageAPI(problem_type)
    print('\n*** C++ style API ***')
    RunLinearExampleCppStyleAPI(problem_type)


if __name__ == '__main__':
  main()
