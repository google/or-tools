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

import argparse
from ortools.constraint_solver.python import constraint_solver as cp

parser = argparse.ArgumentParser()

parser.add_argument(
    '--data', default='examples/contrib/bacp.txt', help='path to data file')

#----------------helper for binpacking posting----------------


def BinPacking(solver, binvars, weights, loadvars):
  """post the load constraint on bins.

  constraints forall j: loadvars[j] == sum_i (binvars[i] == j) * weights[i])
  """
  pack = solver.add_pack(binvars, len(loadvars))
  pack.AddWeightedSumEqualVarDimension(weights, loadvars)
  solver.add(pack)
  solver.add_sum_equality(loadvars, sum(weights))


#------------------------------data reading-------------------


def ReadData(filename):
  """Read data from <filename>."""
  f = open(filename)
  nb_courses, nb_periods, min_credit, max_credit, nb_prereqs =\
      [int(nb) for nb in f.readline().split()]
  credits = [int(nb) for nb in f.readline().split()]
  prereq = [int(nb) for nb in f.readline().split()]
  prereq = [(prereq[i * 2], prereq[i * 2 + 1]) for i in range(nb_prereqs)]
  return (credits, nb_periods, prereq)


def main(args):
  #------------------solver and variable declaration-------------

  credits, nb_periods, prereq = ReadData(args.data)
  nb_courses = len(credits)

  solver = cp.Solver('Balanced Academic Curriculum Problem')

  x = [
      solver.new_int_var(0, nb_periods - 1, 'x' + str(i)) for i in range(nb_courses)
  ]
  load_vars = [
      solver.new_int_var(0, sum(credits), 'load_vars' + str(i))
      for i in range(nb_periods)
  ]

  #-------------------post of the constraints--------------

  # Bin Packing.
  BinPacking(solver, x, credits, load_vars)
  # Add dependencies.
  for i, j in prereq:
    solver.add(x[i] < x[j])

  #----------------Objective-------------------------------

  objective_var = solver.max(load_vars)
  objective = solver.minimize(objective_var, 1)

  #------------start the search and optimization-----------

  db = solver.phase(x, cp.IntVarStrategy.CHOOSE_MIN_SIZE_LOWEST_MIN,
                    cp.IntValueStrategy.INT_VALUE_DEFAULT)

  search_log = solver.search_log(100000, objective_var)
  solver.solve(db, [objective, search_log])


if __name__ == '__main__':
  main(parser.parse_args())
