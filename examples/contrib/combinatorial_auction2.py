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
"""Combinatorial auction in Google CP Solver.

  This is a more general model for the combinatorial example
  in the Numberjack Tutorial, pages 9 and 24 (slides  19/175 and
  51/175).

  The original and more talkative model is here:
  http://www.hakank.org/numberjack/combinatorial_auction.py

  Compare with the following models:
  * MiniZinc: http://hakank.org/minizinc/combinatorial_auction.mzn
  * Gecode: http://hakank.org/gecode/combinatorial_auction.cpp

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
import sys
from collections import *
from ortools.constraint_solver import pywrapcp


def main():

  # Create the solver.
  solver = pywrapcp.Solver("Problem")

  #
  # data
  #
  N = 5

  # the items for each bid
  items = [
      [0, 1],  # A,B
      [0, 2],  # A, C
      [1, 3],  # B,D
      [1, 2, 3],  # B,C,D
      [0]  # A
  ]
  # collect the bids for each item
  items_t = defaultdict(list)

  # [items_t.setdefault(j,[]).append(i) for i in range(N) for j in items[i] ]
  # nicer:
  [items_t[j].append(i) for i in range(N) for j in items[i]]

  bid_amount = [10, 20, 30, 40, 14]

  #
  # declare variables
  #
  X = [solver.BoolVar("x%i" % i) for i in range(N)]
  obj = solver.IntVar(0, 100, "obj")

  #
  # constraints
  #
  solver.Add(obj == solver.ScalProd(X, bid_amount))
  for item in items_t:
    solver.Add(solver.Sum([X[bid] for bid in items_t[item]]) <= 1)

  # objective
  objective = solver.Maximize(obj, 1)

  #
  # solution and search
  #
  solution = solver.Assignment()
  solution.Add(X)
  solution.Add(obj)

  # db: DecisionBuilder
  db = solver.Phase(X, solver.CHOOSE_FIRST_UNBOUND, solver.ASSIGN_MIN_VALUE)

  solver.NewSearch(db, [objective])
  num_solutions = 0
  while solver.NextSolution():
    print("X:", [X[i].Value() for i in range(N)])
    print("obj:", obj.Value())
    print()
    num_solutions += 1

  solver.EndSearch()

  print()
  print("num_solutions:", num_solutions)
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())


if __name__ == "__main__":
  main()
