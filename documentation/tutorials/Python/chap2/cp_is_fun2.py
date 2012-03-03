# Copyright 2010-2011 Google
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
Cryptoarithmetic puzzle

First attempt to solve equation CP + IS + FUN = TRUE
where each letter represents a unique digit.

This problem has 72 different solutions in base 10.

Use of SolutionCollectors.
Use of Solve().
"""

from constraint_solver import pywrapcp
from os import abort

def CPIsFun():
  # Constraint programming engine
  solver = pywrapcp.Solver('CP is fun!');
  
  kBase = 10
  
  # Decision variables
  digits = range(0, kBase)
  digits_without_zero = digits[1:]

  c = solver.IntVar(digits_without_zero, 'C');
  p = solver.IntVar(digits, 'P');
  i = solver.IntVar(digits_without_zero, 'I');
  s = solver.IntVar(digits, 'S');
  f = solver.IntVar(digits_without_zero, 'F');
  u = solver.IntVar(digits, 'U');
  n = solver.IntVar(digits, 'N');
  t = solver.IntVar(digits_without_zero, 'T');
  r = solver.IntVar(digits, 'R');
  e = solver.IntVar(digits, 'E');
  
  # We need to group variables in a list to be able to use
  # the global constraint AllDifferent
  letters = [c, p, i, s, f, u, n, t, r, e]
  
  # Check if we have enough digits
  assert kBase >= len(letters)

  # Constraints
  solver.Add(solver.AllDifferent(letters))
  
  # CP + IS + FUN = TRUE
  term1 = solver.Sum([kBase*c, p])
  term2 = solver.Sum([kBase*i, s])
  term3 = solver.Sum([kBase*kBase*f, kBase*u, n])
  sum_terms = solver.Sum([term1, term2, term3])
  
  sum_value = solver.Sum([kBase*kBase*kBase*t, kBase*kBase*r, kBase*u, e])
  solver.Add(sum_terms == sum_value)
  
  all_solutions = solver.AllSolutionCollector()
  # Add the interesting variables to the SolutionCollector
  all_solutions.Add(c)
  all_solutions.Add(p)
  # Create the variable kBase * c + p
  v1 = solver.Sum([kBase * c, p])
  # Add it to the SolutionCollector
  all_solutions.Add(v1);
  
  db = solver.Phase(letters, solver.INT_VAR_DEFAULT,
                             solver.INT_VALUE_DEFAULT)
  solver.Solve(db, all_solutions)
  
  # Retrieve the solutions
  numberSolutions = all_solutions.SolutionCount()
  
  print "Number of solutions: ", numberSolutions
  
  solution = solver.Assignment()
  
  for index in range(numberSolutions):
    solution = all_solutions.Solution(index)
    print "Solution found:"
    print  "v1=" ,solution.Value(v1)
  
  return

if __name__ == '__main__':
  CPIsFun()
