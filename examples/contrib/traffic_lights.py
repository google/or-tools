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

  Traffic lights problem in Google CP Solver.

  CSPLib problem 16
  http://www.cs.st-andrews.ac.uk/~ianm/CSPLib/prob/prob016/index.html
  '''
  Specification:
  Consider a four way traffic junction with eight traffic lights. Four of the
  traffic
  lights are for the vehicles and can be represented by the variables V1 to V4
  with domains
  {r,ry,g,y} (for red, red-yellow, green and yellow). The other four traffic
  lights are
  for the pedestrians and can be represented by the variables P1 to P4 with
  domains {r,g}.

  The constraints on these variables can be modelled by quaternary constraints
  on
  (Vi, Pi, Vj, Pj ) for 1<=i<=4, j=(1+i)mod 4 which allow just the tuples
  {(r,r,g,g), (ry,r,y,r), (g,g,r,r), (y,r,ry,r)}.

  It would be interesting to consider other types of junction (e.g. five roads
  intersecting) as well as modelling the evolution over time of the traffic
  light sequence.
  ...

  Results
  Only 2^2 out of the 2^12 possible assignments are solutions.

  (V1,P1,V2,P2,V3,P3,V4,P4) =
     {(r,r,g,g,r,r,g,g), (ry,r,y,r,ry,r,y,r), (g,g,r,r,g,g,r,r),
     (y,r,ry,r,y,r,ry,r)}
     [(1,1,3,3,1,1,3,3), ( 2,1,4,1, 2,1,4,1), (3,3,1,1,3,3,1,1), (4,1, 2,1,4,1,
     2,1)}

   The problem has relative few constraints, but each is very tight. Local
   propagation
  appears to be rather ineffective on this problem.

  '''

  Note: In this model we use only the constraint solver.AllowedAssignments().


  Compare with these models:
  * MiniZinc: http://www.hakank.org/minizinc/traffic_lights.mzn
  * Comet   : http://www.hakank.org/comet/traffic_lights.co
  * ECLiPSe : http://www.hakank.org/eclipse/traffic_lights.ecl
  * Gecode  : http://hakank.org/gecode/traffic_lights.cpp
  * SICStus : http://hakank.org/sicstus/traffic_lights.pl


  This model was created by Hakan Kjellerstrand (hakank@gmail.com)
  Also see my other Google CP Solver models:
  http://www.hakank.org/google_or_tools/

"""
import sys

from ortools.constraint_solver import pywrapcp


def main(base=10, start=1, len1=1, len2=4):

  # Create the solver.
  solver = pywrapcp.Solver("Traffic lights")

  #
  # data
  #
  n = 4
  r, ry, g, y = list(range(n))
  lights = ["r", "ry", "g", "y"]

  # The allowed combinations
  allowed = []
  allowed.extend([(r, r, g, g), (ry, r, y, r), (g, g, r, r), (y, r, ry, r)])

  #
  # declare variables
  #
  V = [solver.IntVar(0, n - 1, "V[%i]" % i) for i in range(n)]
  P = [solver.IntVar(0, n - 1, "P[%i]" % i) for i in range(n)]

  #
  # constraints
  #
  for i in range(n):
    for j in range(n):
      if j == (1 + i) % n:
        solver.Add(solver.AllowedAssignments((V[i], P[i], V[j], P[j]), allowed))

  #
  # Search and result
  #
  db = solver.Phase(V + P, solver.INT_VAR_SIMPLE, solver.INT_VALUE_DEFAULT)

  solver.NewSearch(db)
  num_solutions = 0
  while solver.NextSolution():
    for i in range(n):
      print("%+2s %+2s" % (lights[V[i].Value()], lights[P[i].Value()]), end=" ")
    print()
    num_solutions += 1

  solver.EndSearch()

  print()
  print("num_solutions:", num_solutions)
  print("failures:", solver.Failures())
  print("branches:", solver.Branches())
  print("WallTime:", solver.WallTime())
  print()


if __name__ == "__main__":
  main()
