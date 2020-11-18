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

  de Bruijn sequences in Google CP Solver.

  Implementation of de Bruijn sequences in Minizinc, both 'classical' and
  'arbitrary'.
  The 'arbitrary' version is when the length of the sequence (m here) is <
  base**n.


  Compare with the the web based programs:
    http://www.hakank.org/comb/debruijn.cgi
    http://www.hakank.org/comb/debruijn_arb.cgi

  Compare with the following models:
  * Tailor/Essence': http://hakank.org/tailor/debruijn.eprime
  * MiniZinc: http://hakank.org/minizinc/debruijn_binary.mzn
  * SICStus: http://hakank.org/sicstus/debruijn.pl
  * Zinc: http://hakank.org/minizinc/debruijn_binary.zinc
  * Choco: http://hakank.org/choco/DeBruijn.java
  * Comet: http://hakank.org/comet/debruijn.co
  * ECLiPSe: http://hakank.org/eclipse/debruijn.ecl
  * Gecode: http://hakank.org/gecode/debruijn.cpp
  * Gecode/R: http://hakank.org/gecode_r/debruijn_binary.rb
  * JaCoP: http://hakank.org/JaCoP/DeBruijn.java

  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
import sys
from ortools.constraint_solver import pywrapcp

# converts a number (s) <-> an array of numbers (t) in the specific base.


def toNum(solver, t, s, base):
  tlen = len(t)
  solver.Add(
      s == solver.Sum([(base**(tlen - i - 1)) * t[i] for i in range(tlen)]))


def main(base=2, n=3, m=8):
  # Create the solver.
  solver = pywrapcp.Solver("de Bruijn sequences")

  #
  # data
  #
  # base = 2  # the base to use, i.e. the alphabet 0..n-1
  # n    = 3  # number of bits to use (n = 4 -> 0..base^n-1 = 0..2^4 -1, i.e. 0..15)
  # m    = base**n  # the length of the sequence. For "arbitrary" de Bruijn
  # sequences

  # base = 4
  # n    = 4
  # m    = base**n

  # harder problem
  #base = 13
  #n = 4
  #m = 52

  # for n = 4 with different value of base
  # base = 2  0.030 seconds  16 failures
  # base = 3  0.041         108
  # base = 4  0.070         384
  # base = 5  0.231        1000
  # base = 6  0.736        2160
  # base = 7  2.2 seconds  4116
  # base = 8  6 seconds    7168
  # base = 9  16 seconds  11664
  # base = 10 42 seconds  18000
  # base = 6
  # n = 4
  # m = base**n

  # if True then ensure that the number of occurrences of 0..base-1 is
  # the same (and if m mod base = 0)
  check_same_gcc = True

  print("base: %i n: %i m: %i" % (base, n, m))
  if check_same_gcc:
    print("Checks gcc")

  # declare variables
  x = [solver.IntVar(0, (base**n) - 1, "x%i" % i) for i in range(m)]
  binary = {}
  for i in range(m):
    for j in range(n):
      binary[(i, j)] = solver.IntVar(0, base - 1, "x_%i_%i" % (i, j))

  bin_code = [solver.IntVar(0, base - 1, "bin_code%i" % i) for i in range(m)]

  #
  # constraints
  #
  #solver.Add(solver.AllDifferent([x[i] for i in range(m)]))
  solver.Add(solver.AllDifferent(x))

  # converts x <-> binary
  for i in range(m):
    t = [solver.IntVar(0, base - 1, "t_%i" % j) for j in range(n)]
    toNum(solver, t, x[i], base)
    for j in range(n):
      solver.Add(binary[(i, j)] == t[j])

  # the de Bruijn condition
  # the first elements in binary[i] is the same as the last
  # elements in binary[i-i]
  for i in range(1, m - 1):
    for j in range(1, n - 1):
      solver.Add(binary[(i - 1, j)] == binary[(i, j - 1)])

  # ... and around the corner
  for j in range(1, n):
    solver.Add(binary[(m - 1, j)] == binary[(0, j - 1)])

  # converts binary -> bin_code
  for i in range(m):
    solver.Add(bin_code[i] == binary[(i, 0)])

  # extra: ensure that all the numbers in the de Bruijn sequence
  # (bin_code) has the same occurrences (if check_same_gcc is True
  # and mathematically possible)
  gcc = [solver.IntVar(0, m, "gcc%i" % i) for i in range(base)]
  solver.Add(solver.Distribute(bin_code, list(range(base)), gcc))
  if check_same_gcc and m % base == 0:
    for i in range(1, base):
      solver.Add(gcc[i] == gcc[i - 1])

  #
  # solution and search
  #
  solution = solver.Assignment()
  solution.Add([x[i] for i in range(m)])
  solution.Add([bin_code[i] for i in range(m)])
  # solution.Add([binary[(i,j)] for i in range(m) for j in range(n)])
  solution.Add([gcc[i] for i in range(base)])

  db = solver.Phase([x[i] for i in range(m)] + [bin_code[i] for i in range(m)],
                    solver.CHOOSE_MIN_SIZE_LOWEST_MAX, solver.ASSIGN_MIN_VALUE)

  num_solutions = 0
  solver.NewSearch(db)
  num_solutions = 0
  while solver.NextSolution():
    num_solutions += 1
    print("\nSolution %i" % num_solutions)
    print("x:", [int(x[i].Value()) for i in range(m)])
    print("gcc:", [int(gcc[i].Value()) for i in range(base)])
    print("de Bruijn sequence:", [int(bin_code[i].Value()) for i in range(m)])
    # for i in range(m):
    #    for j in range(n):
    #        print binary[(i,j)].Value(),
    #    print
    # print
  solver.EndSearch()

  if num_solutions == 0:
    print("No solution found")

  print()
  print("num_solutions:", num_solutions)
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())


base = 2
n = 3
m = base**n
if __name__ == "__main__":
  if len(sys.argv) > 1:
    base = int(sys.argv[1])
  if len(sys.argv) > 2:
    n = int(sys.argv[2])
  if len(sys.argv) > 3:
    m = int(sys.argv[3])

  main(base, n, m)
