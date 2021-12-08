#!/usr/bin/env python3
#   Copyright 2010 Pierre Schaus pschaus@gmail.com
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.


from ortools.constraint_solver import pywrapcp
from time import time
from random import randint

#----------------helper for binpacking posting----------------


def binpacking(cp, binvars, weights, loadvars):
  """post the connstraints forall j: loadvars[j] == sum_i (binvars[i] == j) * weights[i])"""

  nbins = len(loadvars)
  nitems = len(binvars)
  for j in range(nbins):
    b = [cp.BoolVar(str(i)) for i in range(nitems)]
    for i in range(nitems):
      cp.Add(cp.IsEqualCstCt(binvars[i], j, b[i]))
    cp.Add(solver.Sum([b[i] * weights[i] for i in range(nitems)]) == l[j])
  cp.Add(solver.Sum(loadvars) == sum(weights))

#------------------------------data reading-------------------

maxcapa = 44
weights = [4, 22, 9, 5, 8, 3, 3, 4, 7, 7, 3]
loss = [
    0, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 1, 0, 2, 1, 0, 0, 0, 0, 2, 1, 0,
    0, 0, 0, 0, 0, 0, 0, 1, 0, 2, 1, 0, 3, 2, 1, 0, 2, 1, 0, 0, 0]
nbslab = 11

#------------------solver and variable declaration-------------

solver = pywrapcp.Solver('Steel Mill Slab')
x = [solver.IntVar(0, nbslab-1, 'x' + str(i)) for i in range(nbslab)]
l = [solver.IntVar(0, maxcapa, 'l' + str(i)) for i in range(nbslab)]
obj = solver.IntVar(0, nbslab * maxcapa, 'obj')

#-------------------post of the constraints--------------


binpacking(solver, x, weights[:nbslab], l)
solver.Add(solver.Sum([solver.Element(loss, l[s])
                       for s in range(nbslab)]) == obj)

sol = [2, 0, 0, 0, 0, 1, 2, 2, 1, 1, 2]

#------------start the search and optimization-----------

objective = solver.Minimize(obj, 1)
db = solver.Phase(x, solver.INT_VAR_DEFAULT,
                  solver.INT_VALUE_DEFAULT)
# solver.NewSearch(db,[objective]) #segfault if I comment this

while solver.NextSolution():
  print(obj, 'check:', sum([loss[l[s].Min()] for s in range(nbslab)]))
  print(l)
solver.EndSearch()

print('#fails: ', solver.Failures())
print('time: ', solver.WallTime())
