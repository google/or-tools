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

  Cryptarithmetic puzzle in Google CP Solver.

  Prolog benchmark problem GNU Prolog (crypta.pl)
  '''
  Name           : crypta.pl
  Title          : crypt-arithmetic
  Original Source: P. Van Hentenryck's book
  Adapted by     : Daniel Diaz - INRIA France
  Date           : September 1992

  Solve the operation:

     B A I J J A J I I A H F C F E B B J E A
   + D H F G A B C D I D B I F F A G F E J E
   -----------------------------------------
   = G J E G A C D D H F A F J B F I H E E F
  '''


  Compare with the following models:
  * Comet: http://hakank.org/comet/crypta.co
  * MiniZinc: http://hakank.org/minizinc/crypta.mzn
  * ECLiPSe: http://hakank.org/eclipse/crypta.ecl
  * Gecode: http://hakank.org/gecode/crypta.cpp
  * SICStus: http://hakank.org/sicstus/crypta.pl

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
from ortools.constraint_solver import pywrapcp


def main():

  # Create the solver.
  solver = pywrapcp.Solver("Crypta")

  #
  # data
  #

  #
  # variables
  #
  LD = [solver.IntVar(0, 9, "LD[%i]" % i) for i in range(0, 10)]
  A, B, C, D, E, F, G, H, I, J = LD

  Sr1 = solver.IntVar(0, 1, "Sr1")
  Sr2 = solver.IntVar(0, 1, "Sr2")

  #
  # constraints
  #
  solver.Add(solver.AllDifferent(LD))
  solver.Add(B >= 1)
  solver.Add(D >= 1)
  solver.Add(G >= 1)

  solver.Add(A + 10 * E + 100 * J + 1000 * B + 10000 * B + 100000 * E +
             1000000 * F + E + 10 * J + 100 * E + 1000 * F + 10000 * G +
             100000 * A + 1000000 * F == F + 10 * E + 100 * E + 1000 * H +
             10000 * I + 100000 * F + 1000000 * B + 10000000 * Sr1)

  solver.Add(C + 10 * F + 100 * H + 1000 * A + 10000 * I + 100000 * I +
             1000000 * J + F + 10 * I + 100 * B + 1000 * D + 10000 * I +
             100000 * D + 1000000 * C + Sr1 == J + 10 * F + 100 * A + 1000 * F +
             10000 * H + 100000 * D + 1000000 * D + 10000000 * Sr2)

  solver.Add(A + 10 * J + 100 * J + 1000 * I + 10000 * A + 100000 * B + B +
             10 * A + 100 * G + 1000 * F + 10000 * H + 100000 * D + Sr2 == C +
             10 * A + 100 * G + 1000 * E + 10000 * J + 100000 * G)

  #
  # search and result
  #
  db = solver.Phase(LD, solver.INT_VAR_SIMPLE, solver.INT_VALUE_SIMPLE)

  solver.NewSearch(db)

  num_solutions = 0
  str = "ABCDEFGHIJ"
  while solver.NextSolution():
    num_solutions += 1
    for (letter, val) in [(str[i], LD[i].Value()) for i in range(len(LD))]:
      print("%s: %i" % (letter, val))
    print()

  solver.EndSearch()

  print()
  print("num_solutions:", num_solutions)
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())


if __name__ == "__main__":
  main()
