#   Copyright 2010 Pierre Schaus pschaus@gmail.com, lperron@google.com
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
import argparse
from ortools.constraint_solver.python import constraint_solver as cp
import random

parser = argparse.ArgumentParser()

parser.add_argument(
    '--data',
    default='examples/contrib/steel.txt',
    help='path to data file')
parser.add_argument(
    '--time_limit', default=20000, type=int, help='global time limit')
parser.add_argument(
    '--lns_fragment_size',
    default=10,
    type=int,
    help='size of the random lns fragment')
parser.add_argument(
    '--lns_random_seed',
    default=0,
    type=int,
    help='seed for the lns random generator')
parser.add_argument(
    '--lns_fail_limit',
    default=30,
    type=int,
    help='fail limit when exploring fragments')

# ---------- helper for binpacking posting ----------


def BinPacking(solver, binvars, weights, loadvars):
  """post the load constraint on bins.

  constraints forall j: loadvars[j] == sum_i (binvars[i] == j) * weights[i])
  """
  pack = solver.add_pack(binvars, len(binvars))
  pack.AddWeightedSumEqualVarDimension(weights, loadvars)
  solver.add(pack)
  solver.add_sum_equality(loadvars, sum(weights))


# ---------- data reading ----------


def ReadData(filename):
  """Read data from <filename>."""
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
  loss = [
      min([x for x in capacity if x >= c]) - c for c in range(max_capacity + 1)
  ]
  color_orders = [[o
                   for o in range(nb_slabs)
                   if colors[o] == c]
                  for c in range(1, nb_colors + 1)]
  print('Solving steel mill with', nb_slabs, 'slabs')
  return (nb_slabs, capacity, max_capacity, weights, colors, loss, color_orders)


# ---------- dedicated search for this problem ----------


class SteelDecisionBuilder(cp.PyDecisionBuilder):
  """Dedicated Decision Builder for steel mill slab.

  Search for the steel mill slab problem with Dynamic Symmetry
  Breaking during search is an adaptation (for binary tree) from the
  paper of Pascal Van Hentenryck and Laurent Michel CPAIOR-2008.

  The value heuristic comes from the paper
  Solving Steel Mill Slab Problems with Constraint-Based Techniques:
    CP, LNS, and CBLS,
  Schaus et. al. to appear in Constraints 2010
  """

  def __init__(self, x, nb_slabs, weights, loss_array, loads):
    cp.PyDecisionBuilder.__init__(self)
    self.__x = x
    self.__nb_slabs = nb_slabs
    self.__weights = weights
    self.__loss_array = loss_array
    self.__loads = loads
    self.__max_capacity = len(loss_array) - 1

  def Next(self, solver):
    var, weight = self.NextVar()
    if var:
      v = self.MaxBound()
      if v + 1 == var.min():
        # Symmetry breaking. If you need to assign to a new bin,
        # select the first one.
        solver.add(var == v + 1)
        return self.Next(solver)
      else:
        # value heuristic (important for difficult problem):
        #   try first to place the order in the slab that will induce
        #   the least increase of the loss
        loads = self.getLoads()
        l, v = min(
            (self.__loss_array[loads[i] + weight], i)
            for i in range(var.min(),
                           var.max() + 1)
            if var.Contains(i) and loads[i] + weight <= self.__max_capacity)
        decision = solver.AssignVariabl.value(var, v)
        return decision
    else:
      return None

  def getLoads(self):
    load = [0] * len(self.__loads)
    for (w, x) in zip(self.__weights, self.__x):
      if x.Bound():
        load[x.min()] += w
    return load

  def MaxBound(self):
    """ returns the max value bound to a variable, -1 if no variables bound"""
    return max([-1] + [
        self.__x[o].min()
        for o in range(self.__nb_slabs)
        if self.__x[o].Bound()
    ])

  def NextVar(self):
    """ mindom size heuristic with tie break on the weights of orders """
    res = [(self.__x[o].Size(), -self.__weights[o], self.__x[o])
           for o in range(self.__nb_slabs)
           if self.__x[o].Size() > 1]
    if res:
      res.sort()
      return (res[0][2], -res[0][1])  # returns the order var and its weight
    else:
      return (None, None)

  def DebugString(self):
    return 'SteelMillDecisionBuilder(' + str(self.__x) + ')'


# ----------- LNS Operator ----------


class SteelRandomLns(cp.BaseLns):
  """Random LNS for Steel."""

  def __init__(self, x, rand, lns_size):
    cp.BaseLns.__init__(self, x)
    self.__random = rand
    self.__lns_size = lns_size

  def InitFragments(self):
    pass

  def NextFragment(self):
    while self.FragmentSize() < self.__lns_size:
      pos = self.__random.randint(0, self.Size() - 1)
      self.AppendToFragment(pos)
    return True


# ----------- Main Function -----------


def main(args):
  # ----- solver and variable declaration -----
  (nb_slabs, capacity, max_capacity, weights, colors, loss, color_orders) =\
      ReadData(args.data)
  nb_colors = len(color_orders)
  solver = cp.Solver('Steel Mill Slab')
  x = [solver.new_int_var(0, nb_slabs - 1, 'x' + str(i)) for i in range(nb_slabs)]
  load_vars = [
      solver.new_int_var(0, max_capacity - 1, 'load_vars' + str(i))
      for i in range(nb_slabs)
  ]

  # ----- post of the constraints -----

  # Bin Packing.
  BinPacking(solver, x, weights, load_vars)
  # At most two colors per slab.
  for s in range(nb_slabs):
    solver.add(
        solver.SumLessOrEqual([
            solver.max([solver.add_is_equal_cst_var(x[c], s)
                        for c in o])
            for o in color_orders
        ], 2))

  # ----- Objective -----

  objective_var = \
      solver.sum([load_vars[s].index_of(loss) for s in range(nb_slabs)]).Var()
  objective = solver.minimize(objective_var, 1)

  # ----- start the search and optimization -----

  assign_db = SteelDecisionBuilder(x, nb_slabs, weights, loss, load_vars)
  first_solution = solver.assignment()
  first_solution.add(x)
  first_solution.add_objective(objective_var)
  store_db = solver.StoreAssignment(first_solution)
  first_solution_db = solver.Compose([assign_db, store_db])
  print('searching for initial solution,', end=' ')
  solver.solve(first_solution_db)
  print('initial cost =', first_solution.Objectiv.value())

  # To search a fragment, we use a basic randomized decision builder.
  # We can also use assign_db instead of inner_db.
  inner_db = solver.phase(x, cp.IntVarStrategy.CHOOSE_RANDOM, cp.IntValueStrategy.ASSIGN_MIN_VALUE)
  # The most important aspect is to limit the time exploring each fragment.
  inner_limit = solver.failuresLimit(args.lns_fail_limit)
  continuation_db = solver.SolveOnce(inner_db, [inner_limit])

  # Now, we create the LNS objects.
  rand = random.Random()
  rand.seed(args.lns_random_seed)
  local_search_operator = SteelRandomLns(x, rand, args.lns_fragment_size)
  # This is in fact equivalent to the following predefined LNS operator:
  # local_search_operator = solver.RandomLNSOperator(x,
  #                                                  args.lns_fragment_size,
  #                                                  args.lns_random_seed)
  local_search_parameters = solver.LocalSearchPhaseParameters(
      objective_var, local_search_operator, continuation_db)
  local_search_db = solver.LocalSearc.phase(first_solution,
                                            local_search_parameters)
  global_limit = solver.TimeLimit(args.time_limit)

  print('using LNS to improve the initial solution')

  search_log = solver.search_log(100000, objective_var)
  solver.new_search(local_search_db, [objective, search_log, global_limit])
  while solver.next_solution():
    print('Objective:', objective_var.value(),\
        'check:', sum(loss[load_vars[s].min()] for s in range(nb_slabs)))
  solver.end_search()


if __name__ == '__main__':
  main(parser.parse_args())
