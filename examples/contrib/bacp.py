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
from ortools.constraint_solver import pywrapcp

parser = argparse.ArgumentParser()

parser.add_argument(
    '--data', default='examples/data/bacp/bacp12.txt', help='path to data file')

#----------------helper for binpacking posting----------------


def BinPacking(solver, binvars, weights, loadvars):
  """post the load constraint on bins.

  constraints forall j: loadvars[j] == sum_i (binvars[i] == j) * weights[i])
  """
  pack = solver.Pack(binvars, len(loadvars))
  pack.AddWeightedSumEqualVarDimension(weights, loadvars)
  solver.Add(pack)
  solver.Add(solver.SumEquality(loadvars, sum(weights)))


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

  solver = pywrapcp.Solver('Balanced Academic Curriculum Problem')

  x = [
      solver.IntVar(0, nb_periods - 1, 'x' + str(i)) for i in range(nb_courses)
  ]
  load_vars = [
      solver.IntVar(0, sum(credits), 'load_vars' + str(i))
      for i in range(nb_periods)
  ]

  #-------------------post of the constraints--------------

  # Bin Packing.
  BinPacking(solver, x, credits, load_vars)
  # Add dependencies.
  for i, j in prereq:
    solver.Add(x[i] < x[j])

  #----------------Objective-------------------------------

  objective_var = solver.Max(load_vars)
  objective = solver.Minimize(objective_var, 1)

  #------------start the search and optimization-----------

  db = solver.Phase(x, solver.CHOOSE_MIN_SIZE_LOWEST_MIN,
                    solver.INT_VALUE_DEFAULT)

  search_log = solver.SearchLog(100000, objective_var)
  solver.Solve(db, [objective, search_log])


if __name__ == '__main__':
  main(parser.parse_args())
