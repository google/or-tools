# Copyright 2010-2017 Google
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

"""Rabbit + Pheasant puzzle.

This example is the same one described in
util/constraint/constraint_solver/constraint_solver.h
rewritten using the SWIG generated python wrapper.
Its purpose it to demonstrate how a simple example can be written in all the
flavors of constraint programming interfaces.
"""
from __future__ import print_function
from ortools.constraint_solver import pywrapcp
from ortools.constraint_solver import solver_parameters_pb2


def main():
  parameters = pywrapcp.Solver.DefaultSolverParameters()
  parameters.trace_search = True

  # Create the solver.
  solver = pywrapcp.Solver('rabbit+pheasant', parameters)


  # Create the variables.
  pheasant = solver.IntVar(0, 100, 'pheasant')
  rabbit = solver.IntVar(0, 100, 'rabbit')

  # Create the constraints.
  solver.Add(pheasant + rabbit == 20)
  solver.Add(pheasant * 2 + rabbit * 4 == 56)

  # Create the search phase.
  db = solver.Phase([rabbit, pheasant],
                    solver.INT_VAR_DEFAULT,
                    solver.INT_VALUE_DEFAULT)

  # And solve.
  solver.NewSearch(db)
  solver.NextSolution()

  # Display output.
  print(pheasant)
  print(rabbit)
  solver.EndSearch()
  print(solver)

if __name__ == '__main__':
  main()
