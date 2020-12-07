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

  Ski assignment in Google CP Solver.

  From   Jeffrey Lee Hellrung, Jr.:
  PIC 60, Fall 2008 Final Review, December 12, 2008
  http://www.math.ucla.edu/~jhellrun/course_files/Fall%25202008/PIC%252060%2520-%2520Data%2520Structures%2520and%2520Algorithms/final_review.pdf
  '''
  5. Ski Optimization! Your job at Snapple is pleasant but in the winter
  you've decided to become a ski bum. You've hooked up with the Mount
  Baldy Ski Resort. They'll let you ski all winter for free in exchange
  for helping their ski rental shop with an algorithm to assign skis to
  skiers. Ideally, each skier should obtain a pair of skis whose height
  matches his or her own height exactly. Unfortunately, this is generally
  not possible. We define the disparity between a skier and his or her
  skis to be the absolute value of the difference between the height of
  the skier and the pair of skis. Our objective is to find an assignment
  of skis to skiers that minimizes the sum of the disparities.
  ...
  Illustrate your algorithm by explicitly filling out the A[i, j] table
  for the following sample data:
    * Ski heights: 1, 2, 5, 7, 13, 21.
    * Skier heights: 3, 4, 7, 11, 18.
  '''

  Compare with the following models:
  * Comet   : http://www.hakank.org/comet/ski_assignment.co
  * MiniZinc: http://hakank.org/minizinc/ski_assignment.mzn
  * ECLiPSe : http://www.hakank.org/eclipse/ski_assignment.ecl
  * SICStus: http://hakank.org/sicstus/ski_assignment.pl
  * Gecode: http://hakank.org/gecode/ski_assignment.cpp

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
import sys

from ortools.constraint_solver import pywrapcp


def main():

  # Create the solver.
  solver = pywrapcp.Solver('Ski assignment')

  #
  # data
  #
  num_skis = 6
  num_skiers = 5
  ski_heights = [1, 2, 5, 7, 13, 21]
  skier_heights = [3, 4, 7, 11, 18]

  #
  # variables
  #

  # which ski to choose for each skier
  x = [solver.IntVar(0, num_skis - 1, 'x[%i]' % i) for i in range(num_skiers)]
  z = solver.IntVar(0, sum(ski_heights), 'z')

  #
  # constraints
  #
  solver.Add(solver.AllDifferent(x))

  z_tmp = [
      abs(solver.Element(ski_heights, x[i]) - skier_heights[i])
      for i in range(num_skiers)
  ]
  solver.Add(z == sum(z_tmp))

  # objective
  objective = solver.Minimize(z, 1)

  #
  # search and result
  #
  db = solver.Phase(x, solver.INT_VAR_DEFAULT, solver.INT_VALUE_DEFAULT)

  solver.NewSearch(db, [objective])

  num_solutions = 0
  while solver.NextSolution():
    num_solutions += 1
    print('total differences:', z.Value())
    for i in range(num_skiers):
      x_val = x[i].Value()
      ski_height = ski_heights[x[i].Value()]
      diff = ski_height - skier_heights[i]
      print('Skier %i: Ski %i with length %2i (diff: %2i)' %\
            (i, x_val, ski_height, diff))
    print()

  solver.EndSearch()

  print()
  print('num_solutions:', num_solutions)
  print('failures:', solver.Failures())
  print('branches:', solver.Branches())
  print('WallTime:', solver.WallTime())


if __name__ == '__main__':
  main()
