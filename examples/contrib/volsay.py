# Copyright 2011 Hakan Kjellerstrand hakank@gmail.com
#
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
"""

  Volsay problem in Google or-tools.

  From the OPL model volsay.mod

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
from ortools.linear_solver import pywraplp


def main(unused_argv):

  # Create the solver.

  # using GLPK
  #solver = pywraplp.Solver('CoinsGridGLPK',
  #                         pywraplp.Solver.GLPK_LINEAR_PROGRAMMING)

  # Using CLP
  solver = pywraplp.Solver('CoinsGridCLP',
                           pywraplp.Solver.CLP_LINEAR_PROGRAMMING)

  # data

  # declare variables
  Gas = solver.NumVar(0, 100000, 'Gas')
  Chloride = solver.NumVar(0, 100000, 'Cloride')

  #
  # constraints
  #
  solver.Add(Gas + Chloride <= 50)
  solver.Add(3 * Gas + 4 * Chloride <= 180)

  # objective
  objective = solver.Maximize(40 * Gas + 50 * Chloride)

  print('NumConstraints:', solver.NumConstraints())

  #
  # solution and search
  #
  solver.Solve()

  print()
  print('objective = ', solver.Objective().Value())
  print('Gas = ', Gas.SolutionValue(), 'ReducedCost =', Gas.ReducedCost())
  print('Chloride:', Chloride.SolutionValue(), 'ReducedCost =',
        Chloride.ReducedCost())


if __name__ == '__main__':
  main('Volsay')
