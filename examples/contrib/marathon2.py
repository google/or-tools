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
from ortools.constraint_solver.python import constraint_solver as cp


def main():

  # Create the solver.
  solver = cp.Solver('Marathon')

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
  runners = [solver.new_int_var(1, n, 'runners[%i]' % i) for i in range(n)]
  Dominique, Ignace, Naren, Olivier, Philippe, Pascal = runners

  #
  # constraints
  #
  solver.add_all_different(runners)

  # a: Olivier not last
  solver.add(Olivier != n)

  # b: Dominique, Pascal and Ignace before Naren and Olivier
  solver.add(Dominique < Naren)
  solver.add(Dominique < Olivier)
  solver.add(Pascal < Naren)
  solver.add(Pascal < Olivier)
  solver.add(Ignace < Naren)
  solver.add(Ignace < Olivier)

  # c: Dominique better than third
  solver.add(Dominique < 3)

  # d: Philippe is among the first four
  solver.add(Philippe <= 4)

  # e: Ignace neither second nor third
  solver.add(Ignace != 2)
  solver.add(Ignace != 3)

  # f: Pascal three places earlier than Naren
  solver.add(Pascal + 3 == Naren)

  # g: Neither Ignace nor Dominique on fourth position
  solver.add(Ignace != 4)
  solver.add(Dominique != 4)

  #
  # solution and search
  #
  db = solver.phase(runners, cp.IntVarStrategy.CHOOSE_MIN_SIZE_LOWEST_MIN,
                    cp.IntValueStrategy.ASSIGN_CENTER_VALUE)

  solver.new_search(db)

  num_solutions = 0
  while solver.next_solution():
    num_solutions += 1
    runners_val = [runners[i].value() for i in range(n)]
    print('runners:', runners_val)
    print('Places:')
    for i in range(1, n + 1):
      for j in range(n):
        if runners_val[j] == i:
          print('%i: %s' % (i, runners_str[j]))
    print()

  print('num_solutions:', num_solutions)
  print('failures:', solver.num_failures)
  print('branches:', solver.num_branches)
  print('WallTime:', solver.wall_time_ms, 'ms')


if __name__ == '__main__':
  main()
