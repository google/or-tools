# Copyright 2010 Hakan Kjellerstrand hakank@bonetmail.com
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

  A programming puzzle from Einav in Google CP Solver.

  From
  'A programming puzzle from Einav'
  http://gcanyon.wordpress.com/2009/10/28/a-programming-puzzle-from-einav/
  '''
  My friend Einav gave me this programming puzzle to work on. Given
  this array of positive and negative numbers:
  33   30  -10 -6  18   7  -11 -23   6
  ...
  -25   4  16  30  33 -23  -4   4 -23

  You can flip the sign of entire rows and columns, as many of them
  as you like. The goal is to make all the rows and columns sum to positive
  numbers (or zero), and then to find the solution (there are more than one)
  that has the smallest overall sum. So for example, for this array:
  33  30 -10
  -16  19   9
  -17 -12 -14
  You could flip the sign for the bottom row to get this array:
  33  30 -10
  -16  19   9
  17  12  14
  Now all the rows and columns have positive sums, and the overall total is
  108.
  But you could instead flip the second and third columns, and the second
  row, to get this array:
  33  -30  10
  16   19    9
  -17   12   14
  All the rows and columns still total positive, and the overall sum is just
  66. So this solution is better (I don't know if it's the best)
  A pure brute force solution would have to try over 30 billion solutions.
  I wrote code to solve this in J. I'll post that separately.
  '''

  Compare with the following models:
  * MiniZinc http://www.hakank.org/minizinc/einav_puzzle.mzn
  * SICStus: http://hakank.org/sicstus/einav_puzzle.pl


  This model was created by Hakan Kjellerstrand (hakank@bonetmail.com)
  Also see my other Google CP Solver models: http://www.hakank.org/google_or_tools/
"""

from constraint_solver import pywrapcp


def main():

    # Create the solver.
    solver = pywrapcp.Solver('Einav puzzle')

    #
    # data
    #


    # small problem
    rows = 3;
    cols = 3;
    data = [
        [ 33,  30, -10],
        [-16,  19,   9],
        [-17, -12, -14]
        ]

    #
    # variables
    #
    x = {}
    for i in range(rows):
        for j in range(cols):
            x[i,j] = solver.IntVar(-100, 100, "x[%i,%i]" % (i,j))

    x_flat = [x[i,j] for i in range(rows) for j in range(cols)]

    row_sums = [solver.IntVar(0, 300, "row_sums(%i)" % i) for i in range(rows)]
    col_sums = [solver.IntVar(0, 300, "col_sums(%i)" % j) for j in range(cols)]

    row_signs = [solver.IntVar(-1, 1, "row_signs(%i)" % i) for i in range(rows)]
    col_signs = [solver.IntVar(-1, 1, "col_signs(%i)" % j) for j in range(cols)]

    # total sum: to be minimized
    total_sum = solver.IntVar(0, 1000, "total_sum")

    #
    # constraints
    #
    for i in range(rows):
        for j in range(cols):
            solver.Add(x[i,j] == data[i][j]*row_signs[i]*col_signs[j])

    total_sum_a = [data[i][j]*row_signs[i]*col_signs[j]
                   for i in range(rows) for j in range(cols) ]
    solver.Add(total_sum == solver.Sum(total_sum_a))

    # row sums
    for i in range(rows):
        s = [row_signs[i]*col_signs[j]*data[i][j] for j in range(cols)]
        solver.Add(row_sums[i] == solver.Sum(s))

    # column sums
    for j in range(cols):
        s = [row_signs[i]*col_signs[j]*data[i][j] for i in range(rows)]
        solver.Add(col_sums[j] == solver.Sum(s))


    # objective
    objective = solver.Minimize(total_sum, 1)

    #
    # search and result
    #
    db = solver.Phase(x_flat + row_sums + col_sums,
                 solver.INT_VAR_SIMPLE,
                 solver.INT_VALUE_SIMPLE)

    solver.NewSearch(db, [objective])

    num_solutions = 0
    while solver.NextSolution():
        num_solutions += 1
        print "row_sums:", [row_sums[i].Value() for i in range(rows)]
        print "col_sums:", [col_sums[j].Value() for j in range(cols)]
        for i in range(rows):
            for j in range(cols):
                print x[i,j].Value(),
            print
        print

    solver.EndSearch()

    print
    print "num_solutions:", num_solutions
    print "failures:", solver.failures()
    print "branches:", solver.branches()
    print "wall_time:", solver.wall_time()


if __name__ == '__main__':
    main()
