# Copyright 2010-2013 Google
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


from google.apputils import app

from ortools.linear_solver import pywraplp


def RunLinearExampleNaturalLanguageAPI(optimization_problem_type):
  """Example of simple linear program with natural language API."""
  solver = pywraplp.Solver('RunLinearExampleNaturalLanguageAPI',
                           optimization_problem_type)
  infinity = solver.Infinity()
  # x1, x2 and x3 are continuous non-negative variables.
  x1 = solver.NumVar(0.0, infinity, 'x1')
  x2 = solver.NumVar(0.0, infinity, 'x2')
  x3 = solver.NumVar(0.0, infinity, 'x3')

  solver.Maximize(10 * x1 + 6 * x2 + 4 * x3)
  c0 = solver.Add(x1 + x2 + x3 <= 100.0)
  c1 = solver.Add(10 * x1 + 4 * x2 + 5 * x3 <= 600)
  c2 = solver.Add(2 * x1 + 2 * x2 + 6 * x3 <= 300)

  # TODO(user): Add example that uses sum()

  SolveAndPrint(solver, [x1, x2, x3], [c0, c1, c2])


def RunLinearExampleCppStyleAPI(optimization_problem_type):
  """Example of simple linear program with the C++ style API."""
  solver = pywraplp.Solver('RunLinearExampleCppStyle',
                           optimization_problem_type)
  infinity = solver.Infinity()
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
  print('Number of variables = %d' % solver.NumVariables())
  print('Number of constraints = %d' % solver.NumConstraints())

  result_status = solver.Solve()

  # The problem has an optimal solution.
  assert result_status == pywraplp.Solver.OPTIMAL

  print('Problem solved in %f milliseconds' % solver.WallTime())

  # The objective value of the solution.
  print('Optimal objective value = %f' % solver.Objective().Value())

  # The value of each variable in the solution.
  for variable in variable_list:
    print('%s = %f' % (variable.name(), variable.SolutionValue()))

  print('Advanced usage:')
  print('Problem solved in %d iterations' % solver.Iterations())
  for variable in variable_list:
    print('%s: reduced cost = %f' % (variable.name(), variable.ReducedCost()))
  for i, constraint in enumerate(constraint_list):
    print ('constraint %d: dual value = %f\n'
           '               activity = %f' %
           (i, constraint.DualValue(), constraint.Activity()))


def Announce(solver, api_type):
  print ('---- Linear programming example with ' + solver + ' (' +
         api_type + ') -----')


def RunAllLinearExampleNaturalLanguageAPI():
  if hasattr(pywraplp.Solver, 'GLPK_LINEAR_PROGRAMMING'):
    Announce('GLPK', 'natural language API')
    RunLinearExampleNaturalLanguageAPI(pywraplp.Solver.GLPK_LINEAR_PROGRAMMING)
  if hasattr(pywraplp.Solver, 'CLP_LINEAR_PROGRAMMING'):
    Announce('CLP', 'natural language API')
    RunLinearExampleNaturalLanguageAPI(pywraplp.Solver.CLP_LINEAR_PROGRAMMING)


def RunAllLinearExampleCppStyleAPI():
  if hasattr(pywraplp.Solver, 'GLPK_LINEAR_PROGRAMMING'):
    Announce('GLPK', 'C++ style API')
    RunLinearExampleCppStyleAPI(pywraplp.Solver.GLPK_LINEAR_PROGRAMMING)
  if hasattr(pywraplp.Solver, 'CLP_LINEAR_PROGRAMMING'):
    Announce('CLP', 'C++ style API')
    RunLinearExampleCppStyleAPI(pywraplp.Solver.CLP_LINEAR_PROGRAMMING)


def main(unused_argv):
  RunAllLinearExampleNaturalLanguageAPI()
  RunAllLinearExampleCppStyleAPI()


if __name__ == '__main__':
  app.run()
