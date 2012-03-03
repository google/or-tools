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
This problem has 72 different solutions in base 10.

Use of SolutionCollectors.
Use of Solve().
Use of gflags to choose the base.
Change the time limit of the solver.
"""

import gflags, sys
from constraint_solver import pywrapcp
from os import abort

FLAGS = gflags.FLAGS
gflags.DEFINE_integer('base', 10, "Base used to solve the problem.")
gflags.DEFINE_bool('print_all_solutions', False, "Print all solutions?")
gflags.DEFINE_integer('time_limit', 10000, "Time limit in milliseconds")

def CPIsFun():
  # Use some profiling and change the default parameters of the solver
  solver_params = pywrapcp.SolverParameters()
  # Change the profile level
  solver_params.profile_level = pywrapcp.SolverParameters.NORMAL_PROFILING
  
  # Constraint programming engine
  solver = pywrapcp.Solver('CP is fun!', solver_params);
  
  kBase = gflags.FLAGS.base
  
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
  all_solutions.Add(letters)
  
  db = solver.Phase(letters, solver.INT_VAR_DEFAULT,
                             solver.INT_VALUE_DEFAULT)
  
  # Add some time limit
  time_limit = solver.TimeLimit(gflags.FLAGS.time_limit);
  
  solver.Solve(db, all_solutions, time_limit)
  
  # Retrieve the solutions
  numberSolutions = all_solutions.SolutionCount()
  
  print "Number of solutions: ", numberSolutions
  
  if (gflags.FLAGS.print_all_solutions):
    for index in range(numberSolutions):
      print "C=", all_solutions.Value(index, c), " P=", all_solutions.Value(index, p), " I=", \
      all_solutions.Value(index, i), " S=", all_solutions.Value(index, s), " F=", all_solutions.Value(index, f), \
      " U=", all_solutions.Value(index, u), " N=", all_solutions.Value(index, n), " T=", all_solutions.Value(index, t), \
      " R=", all_solutions.Value(index, r), " E=", all_solutions.Value(index, e)
   
  # Save profile in file
  solver.ExportProfilingOverview("profile.txt")
  
  return

if __name__ == '__main__':
  try:
    FLAGS(sys.argv)  # parse flags
  except gflags.FlagsError, e:
    print '%s\\nUsage: %s ARGS\\n%s' % (e, sys.argv[0], FLAGS)
    sys.exit(1)
  CPIsFun()
