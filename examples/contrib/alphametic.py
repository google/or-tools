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

  Generic alphametic solver in Google CP Solver.

  This is a generic alphametic solver.

  Usage:
     python alphametic.py
                         ->  solves SEND+MORE=MONEY in base 10

     python alphametic.py  'SEND+MOST=MONEY' 11
                         -> solver SEND+MOST=MONEY in base 11

     python alphametic.py TEST <base>
                         -> solve some test problems in base <base>
                            (defined in test_problems())

  Assumptions:
  - we only solves problems of the form
           NUMBER<1>+NUMBER<2>...+NUMBER<N-1> = NUMBER<N>
    i.e. the last number is the sum
  - the only nonletter characters are: +, =, \d (which are splitted upon)


  Compare with the following model:
  * Zinc: http://www.hakank.org/minizinc/alphametic.zinc

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/

"""
import sys
import re

from ortools.constraint_solver import pywrapcp


def main(problem_str="SEND+MORE=MONEY", base=10):

  # Create the solver.
  solver = pywrapcp.Solver("Send most money")

  # data
  print("\nproblem:", problem_str)

  # convert to array.
  problem = re.split("[\s+=]", problem_str)

  p_len = len(problem)
  print("base:", base)

  # create the lookup table: list of (digit : ix)
  a = sorted(set("".join(problem)))
  n = len(a)
  lookup = dict(list(zip(a, list(range(n)))))

  # length of each number
  lens = list(map(len, problem))

  #
  # declare variables
  #

  # the digits
  x = [solver.IntVar(0, base - 1, "x[%i]" % i) for i in range(n)]
  # the sums of each number (e.g. the three numbers SEND, MORE, MONEY)
  sums = [solver.IntVar(1, 10**(lens[i]) - 1) for i in range(p_len)]

  #
  # constraints
  #
  solver.Add(solver.AllDifferent(x))

  ix = 0
  for prob in problem:
    this_len = len(prob)

    # sum all the digits with proper exponents to a number
    solver.Add(
        sums[ix] == solver.Sum([(base**i) * x[lookup[prob[this_len - i - 1]]]
                                for i in range(this_len)[::-1]]))
    # leading digits must be > 0
    solver.Add(x[lookup[prob[0]]] > 0)
    ix += 1

  # the last number is the sum of the previous numbers
  solver.Add(solver.Sum([sums[i] for i in range(p_len - 1)]) == sums[-1])

  #
  # solution and search
  #
  solution = solver.Assignment()
  solution.Add(x)
  solution.Add(sums)

  db = solver.Phase(x, solver.CHOOSE_FIRST_UNBOUND, solver.ASSIGN_MIN_VALUE)

  solver.NewSearch(db)

  num_solutions = 0
  while solver.NextSolution():
    num_solutions += 1
    print("\nsolution #%i" % num_solutions)
    for i in range(n):
      print(a[i], "=", x[i].Value())
    print()
    for prob in problem:
      for p in prob:
        print(p, end=" ")
      print()
    print()
    for prob in problem:
      for p in prob:
        print(x[lookup[p]].Value(), end=" ")
      print()

    print("sums:", [sums[i].Value() for i in range(p_len)])
    print()

  print("\nnum_solutions:", num_solutions)
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())


def test_problems(base=10):
  problems = [
      "SEND+MORE=MONEY", "SEND+MOST=MONEY", "VINGT+CINQ+CINQ=TRENTE",
      "EIN+EIN+EIN+EIN=VIER", "DONALD+GERALD=ROBERT",
      "SATURN+URANUS+NEPTUNE+PLUTO+PLANETS", "WRONG+WRONG=RIGHT"
  ]

  for p in problems:
    main(p, base)


problem = "SEND+MORE=MONEY"
base = 10
if __name__ == "__main__":
  if len(sys.argv) > 1:
    problem = sys.argv[1]
  if len(sys.argv) > 2:
    base = int(sys.argv[2])

  if problem == "TEST" or problem == "test":
    test_problems(base)
  else:
    main(problem, base)
