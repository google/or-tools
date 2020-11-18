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

  Pandigital numbers in Google CP Solver.

  From Albert H. Beiler 'Recreations in the Theory of Numbers',
  quoted from http://www.worldofnumbers.com/ninedig1.htm
  '''
  Chapter VIII : Digits - and the magic of 9

  The following curious table shows how to arrange the 9 digits so that
  the product of 2 groups is equal to a number represented by the
  remaining digits.

     12 x 483 = 5796
     42 x 138 = 5796
     18 x 297 = 5346
     27 x 198 = 5346
     39 x 186 = 7254
     48 x 159 = 7632
     28 x 157 = 4396
     4 x 1738 = 6952
     4 x 1963 = 7852
  '''

  See also MathWorld http://mathworld.wolfram.com/PandigitalNumber.html
  '''
  A number is said to be pandigital if it contains each of the digits
  from 0 to 9 (and whose leading digit must be nonzero). However,
  'zeroless' pandigital quantities contain the digits 1 through 9.
  Sometimes exclusivity is also required so that each digit is
  restricted to appear exactly once.
  '''

  * Wikipedia http://en.wikipedia.org/wiki/Pandigital_number


  Compare with the the following models:
  * MiniZinc: http://www.hakank.org/minizinc/pandigital_numbers.mzn
  * Comet   : http://www.hakank.org/comet/pandigital_numbers.co
  * ECLiPSe : http://www.hakank.org/eclipse/pandigital_numbers.ecl
  * Gecode/R: http://www.hakank.org/gecoder/pandigital_numbers.rb
  * ECLiPSe : http://hakank.org/eclipse/pandigital_numbers.ecl
  * SICStus : http://hakank.org/sicstus/pandigital_numbers.pl

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/

"""
import sys

from ortools.constraint_solver import pywrapcp

#
# converts a number (s) <-> an array of integers (t) in the specific base.
#


def toNum(solver, t, s, base):
  tlen = len(t)
  solver.Add(
      s == solver.Sum([(base**(tlen - i - 1)) * t[i] for i in range(tlen)]))


def main(base=10, start=1, len1=1, len2=4):

  # Create the solver.
  solver = pywrapcp.Solver("Pandigital numbers")

  #
  # data
  #
  max_d = base - 1
  x_len = max_d + 1 - start
  max_num = base**4 - 1

  #
  # declare variables
  #
  num1 = solver.IntVar(0, max_num, "num1")
  num2 = solver.IntVar(0, max_num, "num2")
  res = solver.IntVar(0, max_num, "res")

  x = [solver.IntVar(start, max_d, "x[%i]" % i) for i in range(x_len)]

  #
  # constraints
  #
  solver.Add(solver.AllDifferent(x))

  toNum(solver, [x[i] for i in range(len1)], num1, base)
  toNum(solver, [x[i] for i in range(len1, len1 + len2)], num2, base)
  toNum(solver, [x[i] for i in range(len1 + len2, x_len)], res, base)

  solver.Add(num1 * num2 == res)

  # no number must start with 0
  solver.Add(x[0] > 0)
  solver.Add(x[len1] > 0)
  solver.Add(x[len1 + len2] > 0)

  # symmetry breaking
  solver.Add(num1 < num2)

  #
  # solution and search
  #
  solution = solver.Assignment()
  solution.Add(x)
  solution.Add(num1)
  solution.Add(num2)
  solution.Add(res)

  db = solver.Phase(x, solver.INT_VAR_SIMPLE, solver.INT_VALUE_DEFAULT)

  solver.NewSearch(db)
  num_solutions = 0
  solutions = []
  while solver.NextSolution():
    print_solution([x[i].Value() for i in range(x_len)], len1, len2, x_len)
    num_solutions += 1

  solver.EndSearch()

  if 0 and num_solutions > 0:
    print()
    print("num_solutions:", num_solutions)
    print("failures:", solver.Failures())
    print("branches:", solver.Branches())
    print("WallTime:", solver.WallTime())
    print()


def print_solution(x, len1, len2, x_len):
  print("".join([str(x[i]) for i in range(len1)]), "*", end=" ")
  print("".join([str(x[i]) for i in range(len1, len1 + len2)]), "=", end=" ")
  print("".join([str(x[i]) for i in range(len1 + len2, x_len)]))


base = 10
start = 1
if __name__ == "__main__":
  if len(sys.argv) > 1:
    base = int(sys.argv[1])
  if len(sys.argv) > 2:
    start = int(sys.argv[2])

  x_len = base - 1 + 1 - start
  for len1 in range(1 + (x_len)):
    for len2 in range(1 + (x_len)):
      if x_len > len1 + len2:
        main(base, start, len1, len2)
