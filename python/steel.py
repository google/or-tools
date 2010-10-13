#   Copyright 2010 Pierre Schaus pschaus@gmail.com
#
#   Licensed under the Apache License, Version 2.0 (the 'License');
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an 'AS IS' BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

from google.apputils import app
import gflags
from constraint_solver import pywrapcp
from time import time
from random import randint
import sys

FLAGS = gflags.FLAGS

gflags.DEFINE_string('data', 'python/data/steel_mill/steel_mill_slab.txt',
                     'path to data file')

#----------------helper for binpacking posting----------------


def BinPacking(cp, binvars, weights, loadvars):
  '''post the load constraint on bins.

  constraints forall j: loadvars[j] == sum_i (binvars[i] == j) * weights[i])
  '''
  for j in range(len(loadvars)):
    b = [cp.IsEqualCstVar(binvars[i], j) for i in range(len(binvars))]
    cp.Add(cp.ScalProd(b, weights) == loadvars[j])
  cp.Add(cp.SumEquality(loadvars, sum(weights)))

#------------------------------data reading-------------------

def ReadData(filename):
  '''Read data from <filename>.'''
  f = open(filename)
  capacity = [int(nb) for nb in f.readline().split()]
  capacity.pop(0)
  capacity = [0] + capacity
  max_capacity = max(capacity)
  nb_colors = int(f.readline())
  nb_slabs = int(f.readline())
  wc = [[int(j) for j in f.readline().split()] for i in range(nb_slabs)]
  weights = [x[0] for x in wc]
  colors = [x[1] for x in wc]
  loss = [min(filter(lambda x: x >= c, capacity)) - c
          for c in range(max_capacity + 1)]
  color_orders = [filter(lambda o: colors[o] == c, range(nb_slabs))
                  for c in range(1, nb_colors + 1)]
  print 'Solving steel mill with', nb_slabs, 'slabs'
  return (nb_slabs, capacity, max_capacity, weights, colors, loss, color_orders)

#------------------dedicated search for this problem-----------


class SteelDecisionBuilder(pywrapcp.PyDecisionBuilder):
  '''Dedicated Decision Builder for steel mill slab.

  Search for the steel mill slab problem with Dynamic Symmetry
  Breaking during search (see the paper of Pascal Van Hentenryck and
  Laurent Michel CPAIOR-2008).
  '''
  # TODO: add the value heuristic from the paper Schaus et. al.
  # to appear in Constraints 2010

  def __init__(self, x, nb_slabs, weights):
    self.__x = x
    self.__nb_slabs = nb_slabs
    self.__weights = weights

  def Next(self, solver):
    var = self.NextVar()
    if var:
      v = self.MaxBound()
      if v + 1 == var.Min():
        var.SetValue(v + 1)
        return self.Next(solver)
      else:
        decision = solver.AssignVariableValue(var, var.Min())
        return decision
    else:
      return None

  def MaxBound(self):
    ''' returns the max value bound to a variable, -1 if no variables bound'''
    return max([-1] + [self.__x[o].Min()
                       for o in range(self.__nb_slabs)
                       if self.__x[o].Bound()])

  def NextVar(self):
    ''' mindom size heuristic with tie break on the weights of orders '''
    res = [(self.__x[o].Size(), -self.__weights[o], o, self.__x[o])
           for o in range(self.__nb_slabs)
           if self.__x[o].Size() > 1]
    if res:
      res.sort()
      return res[0][3]
    else:
      return None

  def DebugString(self):
    return 'SteelMillDecisionBuilder(' +  str(self.__x) + ')'


def main(unused_argv):
  #------------------solver and variable declaration-------------
  (nb_slabs, capacity, max_capacity, weights, colors, loss, color_orders) = \
      ReadData(FLAGS.data)
  nb_colors = len(color_orders)
  solver = pywrapcp.Solver('Steel Mill Slab')
  x = [solver.IntVar(0, nb_slabs - 1, 'x' + str(i))
       for i in range(nb_slabs)]
  load_vars = [solver.IntVar(0, max_capacity, 'load_vars' + str(i))
               for i in range(nb_slabs)]

  #-------------------post of the constraints--------------

  # Bin Packing.
  BinPacking(solver, x, weights, load_vars)
  # At most two colors per slab.
  for s in range(nb_slabs):
    solver.Add(solver.SumLessOrEqual(
        [solver.Max([solver.IsEqualCstVar(x[c], s) for c in o])
         for o in color_orders], 2))

  #----------------Objective-------------------------------

  objective_var = \
      solver.Sum([load_vars[s].IndexOf(loss) for s in range(nb_slabs)]).Var()
  objective = solver.Minimize(objective_var, 1)

  #------------start the search and optimization-----------

  db = SteelDecisionBuilder(x, nb_slabs, weights)
  search_log = solver.SearchLog(100000, objective_var)
  solver.NewSearch(db, [objective])
  while solver.NextSolution():
    print 'Objective:', objective_var.Value(),\
        'check:', sum(loss[load_vars[s].Min()] for s in range(nb_slabs)),\
        ', time = ', solver.wall_time(), 'ms'
  solver.EndSearch()
  print 'time =', solver.wall_time(), 'ms'
  print 'failures =', solver.failures()


if __name__ == '__main__':
    main('cp sample')
