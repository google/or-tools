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

  Post office problem in Google CP Solver.

  Problem statement:
  http://www-128.ibm.com/developerworks/linux/library/l-glpk2/

  From Winston 'Operations Research: Applications and Algorithms':
  '''
  A post office requires a different number of full-time employees working
  on different days of the week [summarized below]. Union rules state that
  each full-time employee must work for 5 consecutive days and then receive
  two days off. For example, an employee who works on Monday to Friday
  must be off on Saturday and Sunday. The post office wants to meet its
  daily requirements using only full-time employees. Minimize the number
  of employees that must be hired.

  To summarize the important information about the problem:

    * Every full-time worker works for 5 consecutive days and takes 2 days off
    * Day 1 (Monday): 17 workers needed
    * Day 2 : 13 workers needed
    * Day 3 : 15 workers needed
    * Day 4 : 19 workers needed
    * Day 5 : 14 workers needed
    * Day 6 : 16 workers needed
    * Day 7 (Sunday) : 11 workers needed

  The post office needs to minimize the number of employees it needs
  to hire to meet its demand.
  '''

  * MiniZinc: http://www.hakank.org/minizinc/post_office_problem2.mzn
  * SICStus: http://www.hakank.org/sicstus/post_office_problem2.pl
  * ECLiPSe: http://www.hakank.org/eclipse/post_office_problem2.ecl
  * Gecode: http://www.hakank.org/gecode/post_office_problem2.cpp


  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
from ortools.constraint_solver import pywrapcp


def main():

  # Create the solver.
  solver = pywrapcp.Solver('Post office problem')

  #
  # data
  #

  # days 0..6, monday 0
  n = 7
  days = list(range(n))
  need = [17, 13, 15, 19, 14, 16, 11]

  # Total cost for the 5 day schedule.
  # Base cost per day is 100.
  # Working saturday is 100 extra
  # Working sunday is 200 extra.
  cost = [500, 600, 800, 800, 800, 800, 700]

  #
  # variables
  #

  # No. of workers starting at day i
  x = [solver.IntVar(0, 100, 'x[%i]' % i) for i in days]

  total_cost = solver.IntVar(0, 20000, 'total_cost')
  num_workers = solver.IntVar(0, 100, 'num_workers')

  #
  # constraints
  #
  solver.Add(total_cost == solver.ScalProd(x, cost))
  solver.Add(num_workers == solver.Sum(x))

  for i in days:
    s = solver.Sum(
        [x[j] for j in days if j != (i + 5) % n and j != (i + 6) % n])
    solver.Add(s >= need[i])

  # objective
  objective = solver.Minimize(total_cost, 1)

  #
  # search and result
  #
  db = solver.Phase(x, solver.CHOOSE_MIN_SIZE_LOWEST_MIN,
                    solver.ASSIGN_MIN_VALUE)

  solver.NewSearch(db, [objective])

  num_solutions = 0

  while solver.NextSolution():
    num_solutions += 1
    print('num_workers:', num_workers.Value())
    print('total_cost:', total_cost.Value())
    print('x:', [x[i].Value() for i in days])

  solver.EndSearch()

  print()
  print('num_solutions:', num_solutions)
  print('failures:', solver.Failures())
  print('branches:', solver.Branches())
  print('WallTime:', solver.WallTime())


if __name__ == '__main__':
  main()
