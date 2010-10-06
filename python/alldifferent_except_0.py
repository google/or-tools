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

  All different except 0 Google CP Solver.
  
  Decomposition of global constraint alldifferent_except_0.
  
  From Global constraint catalogue:
  http://www.emn.fr/x-info/sdemasse/gccat/Calldifferent_except_0.html
  '''
  Enforce all variables of the collection VARIABLES to take distinct 
  values, except those variables that are assigned to 0.
  
  Example
     (<5, 0, 1, 9, 0, 3>)
  
  The alldifferent_except_0 constraint holds since all the values 
  (that are different from 0) 5, 1, 9 and 3 are distinct.
  '''

  Compare with the following models:
  * Comet: http://hakank.org/comet/alldifferent_except_0.co
  * ECLiPSe: http://hakank.org/eclipse/alldifferent_except_0.ecl
  * Tailor/Essence': http://hakank.org/tailor/alldifferent_except_0.eprime
  * Gecode: http://hakank.org/gecode/alldifferent_except_0.cpp
  * Gecode/R: http://hakank.org/gecode_r/all_different_except_0.rb
  * MiniZinc: http://hakank.org/minizinc/alldifferent_except_0.mzn
  * SICStus_ http://hakank.org/sicstus/alldifferent_except_0.pl
  * Choco: http://hakank.org/choco/AllDifferentExcept0_test.java
  * JaCoP: http://hakank.org/JaCoP/AllDifferentExcept0_test.java
  * Zinc: http://hakank.org/minizinc/alldifferent_except_0.zinc
  
  This model was created by Hakan Kjellerstrand (hakank@bonetmail.com)
  Also see my other Google CP Solver models: http://www.hakank.org/google_or_tools/

"""




from constraint_solver import pywrapcp

#
# Decomposition of alldifferent_except_0
# Thanks to Laurent Perron (Google) for
# suggestions of improvements.
#
def alldifferent_except_0(solver, a):
    n = len(a)
    for i in range(n):
        for j in range(i):
            bi = solver.IsDifferentCstVar(a[i],0)
            bj = solver.IsDifferentCstVar(a[j],0)
            bij = solver.IsDifferentVar(a[i],a[j])
            solver.Add(bi*bj <= bij)

# more compact version:
def alldifferent_except_0_b(solver, a):
    n = len(a)
    [solver.Add(solver.IsDifferentCstVar(a[i],0)*
              solver.IsDifferentCstVar(a[j],0) <=
              solver.IsDifferentVar(a[i],a[j]))
     for i in range(n) for j in range(i)]




def main(unused_argv):
    # Create the solver.
    solver = pywrapcp.Solver('Alldifferent except 0')

    # data
    n = 7
    
    # declare variables
    x = [solver.IntVar(0,n-1, 'x%i' % i) for i in range(n)]
    z = solver.IntVar(0,n-1, 'z') # number of zeros

    #
    # constraints
    #
    alldifferent_except_0(solver, x)

    # we require 2 0's
    z_tmp = [solver.BoolVar('z_tmp % i' % i) for i in range(n)]
    for i in range(n):
        solver.Add(solver.IsEqualCstCt(x[i], 0, z_tmp[i]))
    solver.Add(solver.Sum(z_tmp) == z)
    solver.Add(z == 2)
    

    #
    # solution and search
    #
    solution = solver.Assignment()
    solution.Add([x[i] for i in range(n)])
    solution.Add(z)               
    
    collector = solver.AllSolutionCollector(solution)
    solver.Solve(solver.Phase([x[i] for i in range(n)],
                              solver.CHOOSE_FIRST_UNBOUND,
                              solver.ASSIGN_MIN_VALUE),
                              [collector])

    num_solutions = collector.solution_count()
    for s in range(num_solutions):
        current = collector.solution(s)
        print "x:", [current.Value(x[i]) for i in range(n)]
        print "z:", current.Value(z)
        print

    print "num_solutions:", num_solutions
    print "failures:", solver.failures()
    print "branches:", solver.branches()
    print "wall_time:", solver.wall_time()
    

if __name__ == '__main__':
    main("cp sample")
