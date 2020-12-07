# Copyright 2010 Hakan Kjellerstrand hakank@gmail.com
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

  Marathon puzzle in Google CP Solver.

  From Xpress example
  http://www.dashoptimization.com/home/cgi-bin/example.pl?id=mosel_puzzle_5_3
  '''
  Dominique, Ignace, Naren, Olivier, Philippe, and Pascal
  have arrived as the first six at the Paris marathon.
  Reconstruct their arrival order from the following
  information:
  a) Olivier has not arrived last
  b) Dominique, Pascal and Ignace have arrived before Naren
     and Olivier
  c) Dominique who was third last year has improved this year.
  d) Philippe is among the first four.
  e) Ignace has arrived neither in second nor third position.
  f) Pascal has beaten Naren by three positions.
  g) Neither Ignace nor Dominique are on the fourth position.

     (c) 2002 Dash Associates
    author: S. Heipcke, Mar. 2002
  '''

  Compare with the following models:
  * MiniZinc: http://www.hakank.org/minizinc/marathon2.mzn
  * SICStus Prolog: http://www.hakank.org/sicstus/marathon2.pl
  * ECLiPSe: http://hakank.org/eclipse/marathon2.ecl
  * Gecode: http://hakank.org/gecode/marathon2.cpp


  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
import sys
from ortools.constraint_solver import pywrapcp


def main():

  # Create the solver.
  solver = pywrapcp.Solver('Marathon')

  #
  # data
  #
  n = 6

  runners_str = [
      'Dominique', 'Ignace', 'Naren', 'Olivier', 'Philippe', 'Pascal'
  ]

  #
  # declare variables
  #
  runners = [solver.IntVar(1, n, 'runners[%i]' % i) for i in range(n)]
  Dominique, Ignace, Naren, Olivier, Philippe, Pascal = runners

  #
  # constraints
  #
  solver.Add(solver.AllDifferent(runners))

  # a: Olivier not last
  solver.Add(Olivier != n)

  # b: Dominique, Pascal and Ignace before Naren and Olivier
  solver.Add(Dominique < Naren)
  solver.Add(Dominique < Olivier)
  solver.Add(Pascal < Naren)
  solver.Add(Pascal < Olivier)
  solver.Add(Ignace < Naren)
  solver.Add(Ignace < Olivier)

  # c: Dominique better than third
  solver.Add(Dominique < 3)

  # d: Philippe is among the first four
  solver.Add(Philippe <= 4)

  # e: Ignace neither second nor third
  solver.Add(Ignace != 2)
  solver.Add(Ignace != 3)

  # f: Pascal three places earlier than Naren
  solver.Add(Pascal + 3 == Naren)

  # g: Neither Ignace nor Dominique on fourth position
  solver.Add(Ignace != 4)
  solver.Add(Dominique != 4)

  #
  # solution and search
  #
  db = solver.Phase(runners, solver.CHOOSE_MIN_SIZE_LOWEST_MIN,
                    solver.ASSIGN_CENTER_VALUE)

  solver.NewSearch(db)

  num_solutions = 0
  while solver.NextSolution():
    num_solutions += 1
    runners_val = [runners[i].Value() for i in range(n)]
    print('runners:', runners_val)
    print('Places:')
    for i in range(1, n + 1):
      for j in range(n):
        if runners_val[j] == i:
          print('%i: %s' % (i, runners_str[j]))
    print()

  print('num_solutions:', num_solutions)
  print('failures:', solver.Failures())
  print('branches:', solver.Branches())
  print('WallTime:', solver.WallTime(), 'ms')


if __name__ == '__main__':
  main()
