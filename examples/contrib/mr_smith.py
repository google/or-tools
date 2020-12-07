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

  Mr Smith in Google CP Solver.

  From an IF Prolog example (http://www.ifcomputer.de/)
  '''
  The Smith family and their three children want to pay a visit but they
  do not all have the time to do so. Following are few hints who will go
  and who will not:
      o If Mr Smith comes, his wife will come too.
      o At least one of their two sons Matt and John will come.
      o Either Mrs Smith or Tim will come, but not both.
      o Either Tim and John will come, or neither will come.
      o If Matt comes, then John and his father will
        also come.
  '''

  The answer should be:
   Mr_Smith_comes      =  0
   Mrs_Smith_comes     =  0
   Matt_comes          =  0
   John_comes          =  1
   Tim_comes           =  1

  Compare with the following models:
  * ECLiPSe: http://www.hakank.org/eclipse/mr_smith.ecl
  * SICStus Prolog: http://www.hakank.org/sicstus/mr_smith.pl
  * Gecode: http://www.hakank.org/gecode/mr_smith.cpp
  * MiniZinc: http://www.hakank.org/minizinc/mr_smith.mzn


  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/
"""
import sys
from ortools.constraint_solver import pywrapcp


def main():

  # Create the solver.
  solver = pywrapcp.Solver('Mr Smith problem')

  #
  # data
  #
  n = 5

  #
  # declare variables
  #
  x = [solver.IntVar(0, 1, 'x[%i]' % i) for i in range(n)]
  Mr_Smith, Mrs_Smith, Matt, John, Tim = x

  #
  # constraints
  #

  #
  # I've kept the MiniZinc constraints for clarity
  # and debugging.
  #

  # If Mr Smith comes then his wife will come too.
  # (Mr_Smith -> Mrs_Smith)
  solver.Add(Mr_Smith - Mrs_Smith <= 0)

  # At least one of their two sons Matt and John will come.
  # (Matt \/ John)
  solver.Add(Matt + John >= 1)

  # Either Mrs Smith or Tim will come but not both.
  # bool2int(Mrs_Smith) + bool2int(Tim) = 1 /\
  # (Mrs_Smith xor Tim)
  solver.Add(Mrs_Smith + Tim == 1)

  # Either Tim and John will come or neither will come.
  # (Tim = John)
  solver.Add(Tim == John)

  # If Matt comes /\ then John and his father will also come.
  # (Matt -> (John /\ Mr_Smith))
  solver.Add(Matt - (John * Mr_Smith) <= 0)

  #
  # solution and search
  #
  db = solver.Phase(x, solver.INT_VAR_DEFAULT, solver.INT_VALUE_DEFAULT)

  solver.NewSearch(db)

  num_solutions = 0
  while solver.NextSolution():
    num_solutions += 1
    print('x:', [x[i].Value() for i in range(n)])

  print()
  print('num_solutions:', num_solutions)
  print('failures:', solver.Failures())
  print('branches:', solver.Branches())
  print('WallTime:', solver.WallTime(), 'ms')


if __name__ == '__main__':
  main()
